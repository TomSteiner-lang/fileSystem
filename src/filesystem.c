
#include "stdlib.h"

#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"
#include "../include/filesystem.h"
//filesystem_create
//filesystem_mount
//filesystem_create_file
//filesystem_delete_file



int filesystem_flush_block(struct filesystem* fs,const void* block ,size_t block_number) {
    if (block_number >= fs->sb->block_count) {
        return -1;
    }
    ssize_t written = disk_write(fs->disk, block,block_number * fs->sb->block_size, fs->sb->block_size);
    if (written < 0) {
        return written;
    }
    if ((size_t)written != fs->sb->block_size) {
        //block is now corrupted on disk
        return -1;
    }

    
    return 0;
}

int filesystem_load_block(struct filesystem* fs,void* block ,size_t block_number) {
    if (block_number >= fs->sb->block_count) {
        return -1;
    }
    ssize_t written = disk_read(fs->disk, block,block_number * fs->sb->block_size, fs->sb->block_size);
    if (written < 0) {
        return written;
    }
    if ((size_t)written != fs->sb->block_size) {
        //block is now corrupted in memory
        return -1;
    }

    return 0;
}

int filesystem_unmount(struct filesystem* fs) {

    //flush all dirty blocks


    size_t bitmap_size = (((fs->sb->block_count + 7) / 8) + fs->sb->block_size - 1) / fs->sb->block_size;
    
    if (bitmap_flush(fs->disk, fs->sb, fs->bm) != bitmap_size) {
        //corrupted disk
        return -1;
    }

    if (superblock_write(fs->disk, fs->sb) < 0) {
        //corrupted disk
        return -1;
    }
    
    bitmap_destroy(fs->bm);
    free(fs->bm);
    free(fs->sb);
    free(fs);

    return 0;
}

struct filesystem* filesystem_mount(struct disk* disk) {
    
    struct filesystem* fs = malloc(sizeof(struct filesystem));
    if (fs == NULL) return NULL;

    struct superblock* sb = malloc(sizeof(struct superblock));
    if (sb == NULL) {
        free(fs);
        return NULL;
    }
    
    struct bitmap* bm = malloc(sizeof(struct bitmap));
    if (bm == NULL) {
        free(fs);
        free(sb);
        return NULL;
    }

    if (superblock_read(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        return NULL;
    }

    if (superblock_validate(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        return NULL;
    }

    if (bitmap_load(disk, sb, bm) < 0) {
        free(fs);
        free(sb);
        free(bm);
        return NULL;
    }

    fs->disk = disk;
    fs->sb = sb;
    fs->bm = bm;
    return fs;
}

int filesystem_create(struct disk* disk, size_t block_size) {
    
    if ((block_size % 1024) || (block_size < 1024)) {
        return -1;
    }

    if (disk->size < FS_MIN_SIZE) {
        return -1;
    }

    size_t blocks = disk->size / block_size;
    if (blocks < FS_MIN_BLOCKS) {
        return -1;
    }
    
    // struct filesystem filesystem = {0};

    // struct filesystem* fs = &filesystem;

    struct superblock superblock = {0};

    struct superblock* sb = &superblock;
    
    struct bitmap bitmap = {0};

    struct bitmap* bm = &bitmap;

    sb->identifier = FS_IDENTIFIER;
    sb->block_size = block_size;
    sb->block_count = blocks;
    sb->bitmap_index = 2;

    size_t bitmap_size = (((blocks + 7) / 8) + block_size - 1) / block_size;
    if (bitmap_create(sb, bm) != 0) {
        return -1;
    }


    sb->root_dir_index = sb->bitmap_index + bitmap_size;


    superblock_write(disk, sb);

    //superblock
    if (bitmap_set(bm, 0) < 0) {
        bitmap_destroy(bm);
        return -1;
    
    }

    //reserved
    if (bitmap_set(bm, 1) < 0) {
        bitmap_destroy(bm);
        return -1;
    
    }
    
    //bitmap
    for (size_t i = 0; i < bitmap_size; i++) {
        if (bitmap_set(bm, sb->bitmap_index + i) < 0) {
            bitmap_destroy(bm);
            return -1;
        }
    }

    //root
    if (bitmap_set(bm, sb->root_dir_index) < 0) {
        bitmap_destroy(bm);
        return -1;
    
    }

    if (bitmap_flush(disk, sb, bm) != bitmap_size) {
        bitmap_destroy(bm);
        return -1;
    }

    //create root dir
    

    return 0;
    
}