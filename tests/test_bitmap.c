#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"
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
        .block_count = 1024,
        .block_size = 1024,
        .root_dir_index = 10,
        .bitmap_index = 2
    };

    struct filesystem fs = {
        .sb = &sb,
        .disk = disk
    };

    assert(!superblock_write(&fs));
    
    struct superblock fromdisk = {0};

    assert(!superblock_read(disk, &fromdisk));
    assert(!superblock_validate(disk, &fromdisk));

    struct bitmap bm = {0};
    
    assert(!bitmap_create(&fromdisk, &bm));
    assert(!bitmap_set(&bm, 0));
    assert(bitmap_is_set(&bm, 0));

    assert(!bitmap_set(&bm, 2));
    assert(bitmap_is_set(&bm, 2));

    //will have to change when the allocation algorithm changes
    size_t allocated = 0;
    assert(!bitmap_allocate(&bm, &allocated));
    assert(allocated == 1);
    assert(!bitmap_allocate(&bm, &allocated));
    assert(allocated == 3);
    assert(!bitmap_allocate(&bm, &allocated));
    assert(allocated == 4);

    assert(bitmap_is_set(&bm, 1));
    assert(bitmap_is_set(&bm, 3));
    assert(bitmap_is_set(&bm, 4));

    assert(!bitmap_free(&bm, 3));
    assert(!bitmap_is_set(&bm, 3));

    assert(!bitmap_allocate(&bm, &allocated));
    assert(allocated == 3);
    assert(bitmap_is_set(&bm, 3));

    fs.bm = &bm;

    assert(bitmap_flush(&fs) == 1);
    

    uint64_t block_count = bm.block_count;
    uint64_t byte_count = bm.byte_count;
    bitmap_destroy(&bm);

    assert(!bitmap_load(&fs));
    assert(bm.byte_count == byte_count);
    assert(bm.block_count == block_count);

    for (int i = 0; i <=4; i++) {
        assert(bitmap_is_set(&bm, i));
    }

    


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}