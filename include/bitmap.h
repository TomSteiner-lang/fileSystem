#ifndef FS_BITMAP
#define FS_BITMAP

#include <stdint.h>

#include "superblock.h"

struct filesystem;

enum marker {
    MARKER_1 = 1,
    MARKER_2 = 2,
    MARKER_UNINITIALIZED = 0,
    MARKER_INVALID = 3
};

enum bitmap_status {
    BITMAP_SUCCESS = 0,
    BITMAP_FAIL = -1,
    BITMAP_INDETERMINATE = -2
};

struct bitmap {
    uint8_t* bits;
    uint8_t* marker;
    int marker_state;
    int prev_state;
    uint64_t block_count;
    uint64_t byte_count;
};

int bitmap_create(const struct superblock* sb, struct bitmap* bm);
int bitmap_allocate(struct bitmap* bm, size_t* out_block);
int bitmap_free(struct bitmap* bm, size_t block);
int bitmap_flush(struct filesystem* fs);
int bitmap_validate_flush(struct filesystem* fs);
void bitmap_destroy(struct bitmap* bm);
int bitmap_load(struct filesystem* fs);
int bitmap_is_set(const struct bitmap* bm, size_t block);
int bitmap_set(struct bitmap* bm, size_t block);




#endif