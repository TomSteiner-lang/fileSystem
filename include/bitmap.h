#ifndef FS_BITMAP
#define FS_BITMAP

#include <stdint.h>

#include "../include/disk.h"
#include "../include/superblock.h"

struct bitmap {
    uint8_t* bits;
    uint64_t block_count;
    uint64_t byte_count;
};

int bitmap_create(const struct superblock* sb, struct bitmap* bm);
int bitmap_allocate(struct bitmap* bm, size_t* out_block);
int bitmap_free(struct bitmap* bm, size_t block);
size_t bitmap_flush(struct disk* disk, const struct superblock* sb, const struct bitmap* bm);
void bitmap_destroy(struct bitmap* bm);
int bitmap_load(const struct disk* disk,const struct superblock* sb, struct bitmap* bm);
int bitmap_is_set(const struct bitmap* bm, size_t block);
int bitmap_set(struct bitmap* bm, size_t block);




#endif