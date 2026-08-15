#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/filesystem.h"

#define TEST_PATH "./images/test.img"
#define TEST_DISK_SIZE 1024 * 1024 * 1

int main(void) {
    unlink(TEST_PATH);

    off_t size = TEST_DISK_SIZE;

    int created = disk_create(TEST_PATH, size);

    assert(!created);
    struct disk * disk = disk_open(TEST_PATH);



    struct superblock sb = {
        .identifier = FS_IDENTIFIER,
        .block_size = 4096,
        .block_count = 64,
        .root_dir_index = 1,
        .bitmap_index = 2
    };

    struct filesystem fs = {
        .sb = &sb,
        .disk = disk,
        .bm = NULL
    };
    assert(!superblock_write(&fs));

    struct superblock fromdisk = {0};

    assert(!superblock_read(disk, &fromdisk));
    assert(!superblock_validate(disk, &fromdisk));

    assert(fromdisk.identifier == FS_IDENTIFIER);
    assert(fromdisk.block_size == 4096);
    assert(fromdisk.block_count == 64);
    assert(fromdisk.root_dir_index == 1);
    assert(fromdisk.bitmap_index == 2);


    


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}