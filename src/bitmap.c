#include <stdio.h>
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

//
int bitmap_create(const struct superblock* sb, struct bitmap* bm) {
    
    uint64_t bytes = sb->block_count / 8;
    if (sb->block_count % 8) {
        bytes++;
    }

    size_t blocks = bytes/sb->block_size;
    if (bytes % sb->block_size) blocks++;

    uint8_t* bits = calloc(1, blocks * sb->block_size);
    if (bits == NULL) return -1;

    uint8_t* marker = calloc(1,sb->block_size);
    if (marker == NULL) {
        free(bits);
        return -1;
    }
    
    

    bm->bits = bits;
    bm->block_count = sb->block_count;
    bm->byte_count = bytes;
    bm->marker = marker;
    bm->marker_state = MARKER_1;
    bm->prev_state = MARKER_1;

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


//
int bitmap_flush(struct filesystem* fs) {
    
    if (fs->bm->marker_state != MARKER_1 && fs->bm->marker_state != MARKER_2) return BITMAP_FAIL;
    fs->bm->prev_state = fs->bm->marker_state;

    
    size_t blocks = fs->bm->byte_count/fs->sb->block_size;
    if (fs->bm->byte_count % fs->sb->block_size != 0) blocks++;
  
    size_t marker_index = blocks + fs->sb->bitmap_index;


    //we want to write to the invalid bitmap and then swap to it
    size_t bitmap_copy = (fs->bm->marker_state == MARKER_1) 
    ? blocks + 1 
    : 0;

    for (size_t i = 0; i < blocks; i++) {
        int written = filesystem_flush_block(fs,
            fs->bm->bits + fs->sb->block_size * i,
            bitmap_copy + fs->sb->bitmap_index + i);
        if (written < 0) return BITMAP_FAIL;
    }
    

    uint8_t new_state = (fs->bm->marker_state == MARKER_1) ? ~0 : 0;
    memset(fs->bm->marker, new_state, fs->sb->block_size);

    
    if (filesystem_flush_block(fs, fs->bm->marker, marker_index) < 0) {
        fs->bm->marker_state = MARKER_INVALID;
        return BITMAP_INDETERMINATE;
    }
    else {

        fs->bm->marker_state = (fs->bm->marker_state == MARKER_1)
        ? MARKER_2
        : MARKER_1;
    }

    return BITMAP_SUCCESS;
}


//
int bitmap_validate_flush(struct filesystem* fs) {
    // on success sets the marker state to the state it is on disk
    // returns whether the last flush was successful or not
    // returns BITMAP_INDETERMINATE on internal failure

    size_t blocks = fs->bm->byte_count/fs->sb->block_size;
    if (fs->bm->byte_count % fs->sb->block_size != 0) blocks++;
  
    size_t marker_index = blocks + fs->sb->bitmap_index;

    if (filesystem_load_block(fs, fs->bm->marker, marker_index) < 0) {
        return BITMAP_INDETERMINATE;
    }

    size_t bitcount = 0;

    for(size_t i = 0; i < fs->sb->block_size; i++) {
        uint8_t byte = fs->bm->marker[i];
        for (int j = 0; j < 8; j++) {
            if (byte % 2 == 1) bitcount++;
            byte >>= 1;
        }
    }

    size_t threshold = fs->sb->block_size * 4 + 1;
    
    fs->bm->marker_state = (bitcount < threshold)
    ? MARKER_1
    : MARKER_2;

    int ret = (fs->bm->marker_state == fs->bm->prev_state)
    ? BITMAP_FAIL
    : BITMAP_SUCCESS;

    return ret;

}

//
void bitmap_destroy(struct bitmap* bm) {
    free(bm->bits);
    free(bm->marker);
    bm->bits = NULL;
    bm->marker = NULL;
    bm->block_count = 0;
    bm->byte_count = 0;
    bm->marker_state = MARKER_UNINITIALIZED;
    bm->prev_state = MARKER_UNINITIALIZED;
    return;
}

//
int bitmap_load(struct filesystem* fs) {

    
    
    
    uint64_t bytes = fs->sb->block_count / 8;
    if (fs->sb->block_count % 8) {
        bytes++;
    }
    size_t blocks = bytes/fs->sb->block_size;
    if (bytes % fs->sb->block_size) blocks++;
    
    
    
    uint8_t* bits = malloc(blocks * fs->sb->block_size);

    if (bits == NULL) return BITMAP_FAIL;


    uint8_t* marker = calloc(1, fs->sb->block_size);
    if (marker == NULL) {
        free(bits);
        return BITMAP_FAIL;
    }




    if (fs->bm->bits != NULL) free(fs->bm->bits);
    fs->bm->bits = bits;
    if (fs->bm->marker != NULL) free(fs->bm->marker);
    fs->bm->marker = marker;
    fs->bm->block_count = fs->sb->block_count;
    fs->bm->byte_count = bytes;




    int* state = &fs->bm->marker_state;
    if (*state == MARKER_INVALID || *state == MARKER_UNINITIALIZED) {
        int res = bitmap_validate_flush(fs);
        if (res == BITMAP_INDETERMINATE) {
            free(bits);
            free(marker);
            return BITMAP_FAIL;
        }
    }


    // if were using bitmap 2 we add an offset to the load
    size_t bitmap_2 = (fs->bm->marker_state == MARKER_2) 
    ? blocks + 1 
    : 0;


    for (size_t i = 0; i < blocks; i++) {
        int res = filesystem_load_block(fs,
            bits + fs->sb->block_size * i,
            bitmap_2 + fs->sb->bitmap_index + i);

        if (res < 0){ 
            free(bits);
            free(marker);
            return BITMAP_FAIL;
        }
    }


    return BITMAP_SUCCESS;
}