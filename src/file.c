#include <stdlib.h>
#include <string.h>

#include "../include/filesystem.h"
#include "../include/file.h"
#include "../include/inode.h"

//file_append_block
//file_pop_block
//file_write_block
//file_read_block


int file_write_block(struct filesystem* fs, struct file* file, void* buff, size_t index) {

    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if (index >= inode->blocks) return -1;

    size_t* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(index_block);
        return -1;
    }

    if (filesystem_flush_block(fs, buff, index_block[index]) < 0) {
        //disk corrupted
        free(index_block);
        return -1;
    }

    free(index_block);
    return 0;
}

int file_read_block(struct filesystem* fs, struct file* file, void* buff, size_t index) {
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if (index >= inode->blocks) return -1;

    size_t* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    void* temp_buff = calloc(1, fs->sb->block_size);
    if (temp_buff == NULL) {
        free(index_block);
        return -1;
    }

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(temp_buff);
        free(index_block);
        return -1;
    }

    if (filesystem_load_block(fs, temp_buff, index_block[index]) < 0) {
        free(temp_buff);
        free(index_block);
        return -1;
    }

    memcpy(buff, temp_buff, fs->sb->block_size);

    free(index_block);
    free(temp_buff);

    return 0;
}

int file_append_block(struct filesystem* fs, struct file* file) {
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if((inode->blocks + 1) * sizeof(size_t) > fs->sb->block_size) {
        return -1;
    }

    size_t* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(index_block);
        return -1;
    }

    size_t newblock = 0;
    if (bitmap_allocate(fs->bm, &newblock) < 0) {
        free(index_block);
        return -1;
    }

    index_block[inode->blocks] = newblock;



    if (filesystem_flush_block(fs, index_block, inode->index) < 0) {
        //disk corrupted
        bitmap_free(fs->bm, newblock);
        free(index_block);
        return -1;
    }


    inode->blocks++;

    
    if (inode_table_flush(fs) < FS_INODE_BLOCKS) {
        inode->blocks--;
        bitmap_free(fs->bm, newblock);
        free(index_block);
        return -1;
    }

    size_t bitmap_size = (((fs->sb->block_count + 7) / 8) + fs->sb->block_size - 1) / fs->sb->block_size;


    if (bitmap_flush(fs) < bitmap_size) {
        //bitmap corrupted
        inode->blocks--;
        inode_table_flush(fs);
        bitmap_free(fs->bm, newblock);
        bitmap_flush(fs);
        free(index_block);
        return -1;
    }


    free(index_block);  
    return 0;


}

int file_pop_block(struct filesystem* fs, struct file* file) {

    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if (inode->blocks == 0) return -1;

    size_t* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(index_block);
        return -1;
    }

    if (bitmap_free(fs->bm, index_block[inode->blocks -1]) < 0) {
        free(index_block);
        return -1;
    }

    inode->blocks--;

    if (inode_table_flush(fs) < FS_INODE_BLOCKS) { 
        inode->blocks++;
        inode_table_flush(fs);
        bitmap_set(fs->bm, index_block[inode->blocks-1]);
        free(index_block);
        return -1;
    }


    size_t bitmap_size = (((fs->sb->block_count + 7) / 8) + fs->sb->block_size - 1) / fs->sb->block_size;


    if (bitmap_flush(fs) < bitmap_size) {
        inode->blocks++;
        inode_table_flush(fs);
        bitmap_set(fs->bm, index_block[inode->blocks-1]);
        bitmap_flush(fs);
        free(index_block);
        return -1;
    }

    free(index_block);
    return 0;

}
