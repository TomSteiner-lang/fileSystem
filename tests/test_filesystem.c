
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include "../include/filesystem.h"


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

    
    assert(fs != NULL);
    assert(fs->disk == disk);
    assert(fs->sb->identifier == FS_IDENTIFIER);
    assert(fs->sb->block_size == TEST_BLOCK_SIZE);
    assert(fs->sb->block_count == TEST_DISK_SIZE / TEST_BLOCK_SIZE);
    // assert(fs->sb->bitmap_index == 2);
    // assert(fs->sb->root_dir_index == 13);



    
    for (int i = 0; i < 4; i++) {
        assert(bitmap_is_set(fs->bm, i));
    }

    assert(!bitmap_is_set(fs->bm, 100));

    bitmap_set(fs->bm, 100);
    assert(bitmap_is_set(fs->bm, 100));


    
    assert(!filesystem_unmount(fs));

    

    fs = filesystem_mount(disk);
    

    
    assert(bitmap_is_set(fs->bm, 100));

    assert(!filesystem_unmount(fs));


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}