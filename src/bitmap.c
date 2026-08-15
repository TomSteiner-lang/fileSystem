
#include <stdlib.h>
#include <string.h>

#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"
#include "../include/filesystem.h"


//bitmap create
//bitmap_allocate
//bitmap_free
//bitmap_load
//bitmap_flush
//bitmap_is_set
//bitmap_set
//bitmap_destroy

int bitmap_create(const struct superblock* sb, struct bitmap* bm) {
    
    uint64_t bytes = sb->block_count / 8;
    if (sb->block_count % 8) {
        bytes++;
    }

    size_t blocks = bytes/sb->block_size;
    if (bytes % sb->block_size) blocks++;

    uint8_t* bits = malloc(blocks * sb->block_size);
    

    if (bits == NULL) return -1;

    
    memset(bits, 0, blocks * sb->block_size);

    bm->bits = bits;
    bm->block_count = sb->block_count;
    bm->byte_count = bytes;


    return 0;
}

int bitmap_allocate(struct bitmap* bm, size_t* out_block) {
    for (size_t i = 0; i < bm->block_count; i++) {
        int state = bitmap_is_set(bm, i);
        if (state < 0) return state;
        if (state == 0) {

            int res = bitmap_set(bm, i);
            if (res < 0) return res;

            *out_block = i;
            return 0;
        }
    }
    return -1;
}

int bitmap_is_set(const struct bitmap* bm, size_t block) {
    if (block >= bm->block_count) return -1;
    uint8_t byte = bm->bits[block/8];
    return !!(byte & (1 << (block % 8)));
}

int bitmap_set(struct bitmap* bm, size_t block) {
    
    int state = bitmap_is_set(bm, block);
    if (state < 0) return state;

    
    //double set, replace -2 with enum
    if (state == 1) return -2;

    uint8_t * byte = &bm->bits[block/8];
    uint8_t mask = (uint8_t)(1u << (block % 8));

    *byte |= mask;

    return 0;
}

int bitmap_free(struct bitmap* bm, size_t block) {
    
    int state = bitmap_is_set(bm, block);
    if (state < 0) return state;


    //double free, replace -3 with enum
    if (state == 0) return -3;

    uint8_t * byte = &bm->bits[block/8];
    uint8_t mask = (uint8_t)~(1u << (block % 8));

    *byte &= mask;

    return 0;
}

size_t bitmap_flush(struct filesystem* fs) {
    //returns amount of blocks successfully written to disk
    size_t blocks = fs->bm->byte_count/fs->sb->block_size;
    if (fs->bm->byte_count % fs->sb->block_size != 0) blocks++;
  
    
    for (size_t i = 0; i < blocks; i++) {
        //block_write(blockno)
        int written = filesystem_flush_block(fs, fs->bm->bits + fs->sb->block_size * i, fs->sb->bitmap_index + i);
        
        if (written < 0) return i;

    }
    return blocks;
}

void bitmap_destroy(struct bitmap* bm) {
    free(bm->bits);
    bm->bits = NULL;
    bm->block_count = 0;
    bm->byte_count = 0;
    return;
}

int bitmap_load(struct filesystem* fs) {

    uint64_t bytes = fs->sb->block_count / 8;
    if (fs->sb->block_count % 8) {
        bytes++;
    }
    size_t blocks = bytes/fs->sb->block_size;
    if (bytes % fs->sb->block_size) blocks++;

    uint8_t* bits = malloc(blocks * fs->sb->block_size);

    if (bits == NULL) return -1;

    for (size_t i = 0; i < blocks; i++) {
        int res = filesystem_load_block(fs, bits + fs->sb->block_size * i, fs->sb->bitmap_index + i);
        if (res < 0){ 
            free(bits);
            return res;
        }
    }

    fs->bm->bits = bits;
    fs->bm->block_count = fs->sb->block_count;
    fs->bm->byte_count = bytes;


    return 0;
}