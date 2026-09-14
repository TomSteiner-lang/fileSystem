#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"
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

    assert(fs->bm->marker_state == MARKER_1 || fs->bm->marker_state == MARKER_2);
    int state = fs->bm->marker_state;
    size_t allocated[5] = {0};
    for (int i = 0; i < 5; i++) {
        assert(bitmap_allocate(fs->bm, allocated + i) == 0);
    }

    assert(bitmap_flush(fs) == BITMAP_SUCCESS);
    
    assert(fs->bm->marker_state == MARKER_1 || fs->bm->marker_state == MARKER_2);
    assert(fs->bm->marker_state != state);
    assert(fs->bm->prev_state == state);

    assert(bitmap_validate_flush(fs) == BITMAP_SUCCESS);
    assert(fs->bm->marker_state != state);
    assert(fs->bm->prev_state == state);

    state = fs->bm->marker_state;

    size_t wont_allocate = 0;
    assert(bitmap_allocate(fs->bm, &wont_allocate) == 0);
    disk->fd += 10;
    assert(bitmap_flush(fs) == BITMAP_FAIL);
    assert(fs->bm->marker_state == state);
    assert(bitmap_validate_flush(fs) == BITMAP_INDETERMINATE);
    assert(fs->bm->marker_state == state);

    disk->fd -= 10;
    
    assert(bitmap_validate_flush(fs) == BITMAP_FAIL);
    assert(fs->bm->marker_state == state);

    


    assert(filesystem_unmount(fs) == 0);

    struct filesystem* fs2 = filesystem_mount(disk);
    
    assert(filesystem_unmount(fs2) == 0);
    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;

}