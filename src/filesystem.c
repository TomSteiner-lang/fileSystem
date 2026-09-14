
#include <stdlib.h>
#include <string.h>

#include "../include/disk.h"
#include "../include/superblock.h"
#include "../include/bitmap.h"
#include "../include/filesystem.h"
#include "../include/file.h"


//filesystem_create
//filesystem_mount
//filesystem_load_block
//filesystem_flush_block
//filesystem_create_file
//filesystem_delete_file

struct file* filesystem_create_file(struct filesystem* fs, int type) {
    
    struct file* file = calloc(1, sizeof(struct file));
    if (file == NULL) {
        return NULL;
    }


    size_t inode_index = inode_table_add(fs->it, type);
    if (inode_index == 0) {
        free(file);
        return NULL;
    }

    struct inode* inode = inode_table_get(fs->it, inode_index);
    if (inode == NULL) {
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }

    
    size_t index_block = 0;
    if (bitmap_allocate(fs->bm, &index_block) < 0) {
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }

    inode->index = index_block;
    size_t data_block = 0;
    if (bitmap_allocate(fs->bm, &data_block) < 0) {
        bitmap_free(fs->bm, index_block);
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }

    inode->blocks = 1;
    file->inode = inode_index;
 
    size_t* index = calloc(1, fs->sb->block_size);
    if (index == NULL) {
        bitmap_free(fs->bm, data_block);
        bitmap_free(fs->bm, index_block);
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }

    index[0] = data_block;

    if (filesystem_flush_block(fs, index, index_block) < 0) {
        free(index);
        bitmap_free(fs->bm, data_block);
        bitmap_free(fs->bm, index_block);
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }
    free(index);

    
    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {        
        bitmap_free(fs->bm, data_block);
        bitmap_free(fs->bm, index_block);
        inode_table_remove(fs->it, inode_index);
        free(file);
        return NULL;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic faliure recovery
        free(file);
        return NULL;
    }

    if (inode_table_flush(fs) < FS_INODE_BLOCKS) {
               
        bitmap_free(fs->bm, data_block);
        bitmap_free(fs->bm, index_block);
        bitmap_flush(fs);
        inode_table_remove(fs->it, inode_index);
        inode_table_flush(fs);
        free(file);
        return NULL;
    }

    return file;

}

int filesystem_delete_file(struct filesystem* fs, struct file* file) {
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    struct inode inode_copy = *inode; 

    size_t* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    int loaded = filesystem_load_block(fs, (void*)index_block, inode->index);

    if (loaded < 0) {
        free(index_block);
        return -1;
    }

    for (size_t i = 0; i < inode->blocks; i++) {
        int freed = bitmap_free(fs->bm, index_block[i]);
        if (freed < 0) {
            for (size_t j = 0; j < i; j++) {
                bitmap_set(fs->bm, index_block[j]);
            }
            free(index_block);
            return -1;
        }
    }

    if (bitmap_free(fs->bm, inode->index) < 0) {
        for (size_t i = 0; i < inode->blocks; i++) {
            bitmap_set(fs->bm, index_block[i]);
        }
        free(index_block);
        return -1;
    }
    


    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        bitmap_set(fs->bm, inode->index);
        for (size_t i = 0; i < inode->blocks; i++) {
            bitmap_set(fs->bm, index_block[i]);
        }
        free(index_block);
        return -1;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic faliure recovery
        free(index_block);
        return -1;
    }


    inode_table_remove(fs->it, file->inode);

    if (inode_table_flush(fs) < FS_INODE_BLOCKS) {
        inode_table_set(fs->it, &inode_copy, file->inode);
        bitmap_set(fs->bm, inode_copy.index);
        for (size_t i = 0; i < inode_copy.blocks; i++) {
            bitmap_set(fs->bm, index_block[i]);
        }
        bitmap_flush(fs);
        free(index_block);
        return -1;
    }

    free(index_block);
    free(file);
    return 0;


}

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
//todo - make transactional
    //flush all dirty blocks


    if (inode_table_flush(fs) != FS_INODE_BLOCKS) {
        //corrupted disk
        return -1;
    }

    
    int res = bitmap_flush(fs);
    

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        //rollback
        return -1;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic faliure recovery
        return -1;
    }

    if (superblock_write(fs) < 0) {
        //corrupted disk
        return -1;
    }
    
    inode_table_destroy(fs->it);
    free(fs->it);
    bitmap_destroy(fs->bm);
    free(fs->bm);
    free(fs->sb);
    

    memset(fs, 0, sizeof(struct filesystem));


    free(fs);
    return 0;
}

struct filesystem* filesystem_mount(struct disk* disk) {
    
    struct filesystem* fs = calloc(1, sizeof(struct filesystem));
    if (fs == NULL) return NULL;

    struct superblock* sb = calloc(1, sizeof(struct superblock));
    if (sb == NULL) {
        free(fs);
        return NULL;
    }
    
    struct bitmap* bm = calloc(1, sizeof(struct bitmap));
    if (bm == NULL) {
        free(fs);
        free(sb);
        return NULL;
    }

    struct inode_table* it = calloc(1, sizeof(struct inode_table));
    if (it == NULL) {
        free(fs);
        free(sb);
        free(bm);
        return NULL;
    }
    
    fs->disk = disk;
    fs->sb = sb;
    fs->bm = bm;
    fs->it = it;

    if (superblock_read(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        free(it);
        return NULL;
    }

    if (superblock_validate(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        free(it);
        return NULL;
    }

    if (bitmap_load(fs) != BITMAP_SUCCESS) {
    
    
        free(fs);
        free(sb);
        free(bm);
        free(it);
        return NULL;
    }

    

    if (inode_table_load(fs) < 0) {
        free(fs);
        free(sb);
        bitmap_destroy(bm);
        free(bm);
        free(it);
        return NULL;
    }

    return fs;
}


//
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
    
    struct filesystem filesystem = {0};

    struct filesystem* fs = &filesystem;

    struct superblock superblock = {0};

    struct superblock* sb = &superblock;
    
    struct bitmap bitmap = {0};

    struct bitmap* bm = &bitmap;

    struct inode_table inode_table = {0};

    struct inode_table* it = &inode_table;

    fs->bm = bm;
    fs->sb = sb;
    fs->disk = disk;
    fs->it = it;

    sb->identifier = FS_IDENTIFIER;
    sb->block_size = block_size;
    sb->block_count = blocks;
    sb->bitmap_index = 2;
    

    size_t bitmap_size = (((blocks + 7) / 8) + block_size - 1) / block_size;
    if (bitmap_create(sb, bm) != 0) {
        return -1;
    }

    size_t bitmap_area_size = 2 * bitmap_size + 1;

    sb->inode_table_index = sb->bitmap_index + bitmap_area_size;
    sb->inode_table_size = FS_INODE_BLOCKS;
    sb->root_dir_index = sb->inode_table_index + FS_INODE_BLOCKS;

    if(inode_table_create(sb,it) < 0) {
        bitmap_destroy(bm);
        return -1;
    }


    if (superblock_write(fs) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    }

    //superblock
    if (bitmap_set(bm, 0) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    
    }

    //reserved
    if (bitmap_set(bm, 1) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    
    }
    
    //bitmap
    for (size_t i = 0; i < bitmap_area_size; i++) {
        if (bitmap_set(bm, sb->bitmap_index + i) < 0) {
            bitmap_destroy(bm);
            inode_table_destroy(it);
            return -1;
        }
    }

    //inode table
    for (size_t i = 0; i < FS_INODE_BLOCKS; i++) {
        if (bitmap_set(bm, sb->inode_table_index + i) < 0) {
            bitmap_destroy(bm);
            inode_table_destroy(it);
            return -1;
        }
    }

    //root
    if (bitmap_set(bm, sb->root_dir_index) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    
    }


    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res != BITMAP_SUCCESS) {
        //dont care about indeterminate because the filesystem is empty
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    }

    //create root dir
    

    return 0;
    
}