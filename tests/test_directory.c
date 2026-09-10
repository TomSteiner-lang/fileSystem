#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../include/filesystem.h"
#include "../include/file.h"
#include "../include/directory.h"


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


    struct file* dir = filesystem_create_file(fs, INODE_DIRECTORY);

    assert(dir != NULL);

    size_t fileindex = 0;
    assert(directory_lookup(fs, dir, "test1", &fileindex) < 0);

    size_t files = 5;

    struct file* testfiles[files];

    for (size_t i = 0; i < files; i++) {
        testfiles[i] = filesystem_create_file(fs, INODE_FILE);
        assert(testfiles[i] != NULL);

        char* path = calloc(1,2);
        assert(path != NULL);
        *path = 'a' + i;

        assert(directory_add(fs, dir, testfiles[i], path) == 0);

    }

    struct file* found = directory_get_file(fs, dir, "c");
    assert(found != NULL);
    assert(found->inode == testfiles[2]->inode);

    found = directory_get_file(fs, dir, "e");
    assert(found != NULL);
    assert(found->inode == testfiles[4]->inode);

    assert(directory_remove(fs, dir, "c") == 0);
    assert(directory_get_file(fs, dir, "c") == NULL);



    


    
    assert(!filesystem_unmount(fs));


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}