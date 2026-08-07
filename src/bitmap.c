
#include <stdlib.h>
#include <string.h>

#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"


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

size_t bitmap_flush(struct disk* disk, const struct superblock* sb, const struct bitmap* bm) {
    //returns amount of blocks successfully written to disk
    size_t blocks = bm->byte_count/sb->block_size;
    if (bm->byte_count % sb->block_size != 0) blocks++;
  
    
    for (size_t i = 0; i < blocks; i++) {
        //block_write(blockno)
        ssize_t written = disk_write(disk, bm->bits + i * sb->block_size, (off_t)(sb->bitmap_index + i) * sb->block_size, sb->block_size);
        
        if (written < 0) return i;

        if ((uint64_t) written != sb->block_size) return i;
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

int bitmap_load(const struct disk* disk,const struct superblock* sb, struct bitmap* bm) {

    uint64_t bytes = sb->block_count / 8;
    if (sb->block_count % 8) {
        bytes++;
    }
    size_t blocks = bytes/sb->block_size;
    if (bytes % sb->block_size) blocks++;

    uint8_t* bits = malloc(blocks * sb->block_size);

    if (bits == NULL) return -1;

    ssize_t read = disk_read(disk, bits, sb->block_size * sb->bitmap_index,blocks * sb->block_size);
    if (read < 0) return -1;
    if ((uint64_t) read != blocks * sb->block_size) {
        free(bits);
        return -1;
    }

    bm->bits = bits;
    bm->block_count = sb->block_count;
    bm->byte_count = bytes;


    return 0;
}