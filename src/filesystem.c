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
    


    //mechanism:
    //allocate block for index
    //wipe the index block - fail: remove bitmap allocation from ram
    //get inode - fail: same as above
    //flush inode with type invalid - fail: remove bitmap allocation and inode from ram
    //flush bitmap - fail: remove only bitmap allocation from ram, recovery handles the inode
    //flush inode with real type - fail: nothing, recovery cleans garbage inode



    if (type == INODE_FREE || type == INODE_INVALID) return NULL;

    struct file* file = calloc(1, sizeof(struct file));
    if (file == NULL) {
        return NULL;
    }
    void* block_buffer = calloc(1, fs->sb->block_size);
    if (block_buffer == NULL) {
        free(file);
        return NULL;
    }

    size_t index_block = 0;
    if (bitmap_allocate(fs->bm, &index_block) < 0) {
        free(block_buffer);
        free(file);
        return NULL;
    }


    if (filesystem_flush_block(fs, block_buffer, index_block) < 0) {
        bitmap_free(fs->bm, index_block);
        free(block_buffer);
        free(file);
        return NULL;
    }

    size_t inode_index = inode_table_add(fs->it, INODE_INVALID);
    if (inode_index == 0) {
        bitmap_free(fs->bm, index_block);
        free(block_buffer);
        free(file);
        return NULL;
    }

    struct inode inode = {
        .blocks = 0,
        .entries = 0,
        .index = index_block,
        .type = INODE_INVALID
    };

    if (inode_table_set(fs->it, &inode, inode_index) < 0) {
        inode_table_remove(fs->it, inode_index);
        bitmap_free(fs->bm, index_block);
        free(block_buffer);
        free(file);
        return NULL;
    }


    if (inode_table_flush(fs) < 0) {
        inode_table_remove(fs->it, inode_index);
        bitmap_free(fs->bm, index_block);
        free(block_buffer);
        free(file);
        return NULL;
    }


    if (bitmap_flush(fs) < 0) {
        bitmap_free(fs->bm, index_block);
        free(block_buffer);
        free(file);
        return NULL;
    }


    inode.type = type;
    if (inode_table_set(fs->it, &inode, inode_index) < 0) {
        //currently can never happen because only way this fails is boundary checks that already passed
        free(block_buffer);
        free(file);
        return NULL;
    }


    if (inode_table_flush(fs) < 0) {
        free(block_buffer);
        free(file);
        return NULL;
    }


    file->inode = inode_index;
    return file;

}

int filesystem_delete_file(struct filesystem* fs, struct file* file) {
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    struct inode inode_copy = *inode; 

    struct file_block_entry* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    int loaded = filesystem_load_block(fs, (void*)index_block, inode->index);

    if (loaded < 0) {
        free(index_block);
        return -1;
    }


    inode_copy.type = INODE_INVALID;
    if (inode_table_set(fs->it, &inode_copy, file->inode) < 0) {
        free(index_block);
        return -1;
    }
    
    
    if (inode_table_flush(fs) < 0) {
        free(index_block);
        return -1;
    }

    for (size_t i = 0; i < inode->blocks; i++) {
        int freed = bitmap_free(fs->bm, index_block[i].block_number);
        if (freed < 0) {
            for (size_t j = 0; j < i; j++) {
                bitmap_set(fs->bm, index_block[j].block_number);
            }
            free(index_block);
            return -2;
        }
    }

    if (bitmap_free(fs->bm, inode->index) < 0) {
        for (size_t i = 0; i < inode->blocks; i++) {
            bitmap_set(fs->bm, index_block[i].block_number);
        }
        free(index_block);
        return -2;
    }
    


    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        bitmap_set(fs->bm, inode->index);
        for (size_t i = 0; i < inode->blocks; i++) {
            bitmap_set(fs->bm, index_block[i].block_number);
        }
        free(index_block);
        return -2;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic faliure recovery
        free(index_block);
        return -2;
    }


    inode_table_remove(fs->it, file->inode);

    if (inode_table_flush(fs) < 0) {
        free(index_block);
        return -2;
    }

    free(index_block);
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



    //resolve all transactions




    if (inode_table_flush(fs) != 0) {
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
    
    transaction_table_destroy(fs->tt);
    free(fs->tt);
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

    struct transaction_table* tt = calloc(1, sizeof(struct transaction_table));
    if (tt == NULL) {
        free(fs);
        free(sb);
        free(bm);
        free(it);
        return NULL;
    }
    
    fs->disk = disk;
    fs->sb = sb;
    fs->bm = bm;
    fs->it = it;
    fs->tt = tt;

    if (superblock_read(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        free(it);
        free(tt);
        return NULL;
    }

    if (superblock_validate(disk, sb) < 0) {
        free(fs);
        free(sb);
        free(bm);
        free(it);
        free(tt);
        return NULL;
    }

    if (bitmap_load(fs) != BITMAP_SUCCESS) {
    
    
        free(fs);
        free(sb);
        free(bm);
        free(it);
        free(tt);
        return NULL;
    }

    //cleanup all of the transactions

    if (inode_table_load(fs) < 0) {
        free(fs);
        free(sb);
        bitmap_destroy(bm);
        free(bm);
        free(it);
        free(tt);
        return NULL;
    }

    if (transaction_table_create(sb, tt) < 0) {
        free(fs);
        free(sb);
        bitmap_destroy(bm);
        inode_table_destroy(it);
        free(bm);
        free(it);
        free(tt);
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

    struct transaction_table transaction_table = {0};

    struct transaction_table* tt = &transaction_table;


    fs->bm = bm;
    fs->sb = sb;
    fs->disk = disk;
    fs->it = it;
    fs->tt = tt;

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
    sb->transaction_table_index = sb->inode_table_index + sb->inode_table_size;
    sb->transaction_table_size = FS_TRANSACTION_BLOCKS;
    sb->root_dir_index = sb->transaction_table_index + sb->transaction_table_size;

    if(inode_table_create(sb,it) < 0) {
        bitmap_destroy(bm);
        return -1;
    }


    if (transaction_table_create(sb, tt) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        return -1;
    }

    if (superblock_write(fs) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        transaction_table_destroy(tt);
        return -1;
    }

    //superblock
    if (bitmap_set(bm, 0) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        transaction_table_destroy(tt);
        return -1;
    
    }

    //reserved
    if (bitmap_set(bm, 1) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        transaction_table_destroy(tt);
        return -1;
    
    }
    
    //bitmap
    for (size_t i = 0; i < bitmap_area_size; i++) {
        if (bitmap_set(bm, sb->bitmap_index + i) < 0) {
            bitmap_destroy(bm);
            inode_table_destroy(it);
            transaction_table_destroy(tt);
            return -1;
        }
    }

    //inode table
    for (size_t i = 0; i < FS_INODE_BLOCKS; i++) {
        if (bitmap_set(bm, sb->inode_table_index + i) < 0) {
            bitmap_destroy(bm);
            inode_table_destroy(it);
            transaction_table_destroy(tt);
            return -1;
        }
    }

    for (size_t i = 0; i < sb->transaction_table_size; i++) {
        if (bitmap_set(bm, sb->transaction_table_index +i) < 0) {
            bitmap_destroy(bm);
            inode_table_destroy(it);
            transaction_table_destroy(tt);
            return -1;
        }
    }

    //root
    if (bitmap_set(bm, sb->root_dir_index) < 0) {
        bitmap_destroy(bm);
        inode_table_destroy(it);
        transaction_table_destroy(tt);

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
        transaction_table_destroy(tt);

        return -1;
    }

    //create root dir
    

    return 0;
    
}