
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "../include/filesystem.h"
#include "../include/inode_table.h"

#define TEST_PATH "./images/test.img"
#define TEST_DISK_SIZE 1024 * 1024 * 1
#define TEST_BLOCK_SIZE 1024


int main(void) {
    unlink(TEST_PATH);

    off_t size = TEST_DISK_SIZE;

    int created = disk_create(TEST_PATH, size);

    assert(!created);
    struct disk * disk = disk_open(TEST_PATH);

    
    assert(filesystem_create(disk, TEST_BLOCK_SIZE) == 0);

    struct filesystem* fs = filesystem_mount(disk);

    assert(inode_table_add(fs->it, INODE_FILE) == 1);
    
    assert(inode_table_add(fs->it, INODE_DIRECTORY) == 2);
    
    assert(!filesystem_unmount(fs));

    struct filesystem* fs2 = filesystem_mount(disk);

    assert(inode_table_add(fs2->it, INODE_FILE) == 3);

    inode_table_remove(fs2->it, 2);
    assert(inode_table_add(fs2->it, INODE_DIRECTORY) == 2);

    struct inode* inode = inode_table_get(fs2->it, 2);

    assert(inode->type == INODE_DIRECTORY);
    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}