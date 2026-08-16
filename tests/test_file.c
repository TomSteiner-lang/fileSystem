
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../include/filesystem.h"
#include "../include/file.h"


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


    struct file* file = filesystem_create_file(fs, INODE_DIRECTORY);

    assert(file != NULL);
    size_t inode_index = file->inode;

    struct inode* inode = inode_table_get(fs->it, inode_index);
    assert(inode->type == INODE_DIRECTORY);
    assert(bitmap_is_set(fs->bm, inode->index));
    
    size_t* index_block = calloc(1, fs->sb->block_size);

    if (index_block == NULL) {
        printf("test failed because of malloc\n");
        assert(!filesystem_unmount(fs));


        assert(!disk_close(disk));

        assert(!unlink(TEST_PATH));
        
        return 1;
    }

    assert(filesystem_load_block(fs, index_block,inode_index) == 0);

    assert(bitmap_is_set(fs->bm, index_block[0]));

    free(index_block);

    assert(!filesystem_unmount(fs));


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}