
#include <stdlib.h>
#include <string.h>
#include "../include/file.h"
#include "../include/filesystem.h"
#include "../include/directory.h"

//directory_add
//directory_remove
//directory_get
//directory_lookup

//last entry:
//size = 16
//entries = 15
//first empty block = size/entries
//first empty entry = size%entries 

int directory_add(struct filesystem* fs, struct file* directory, struct file* file, char* path) {
    
    struct directory_entry burner = {0};
    size_t max_path_length = sizeof(burner.name);
    if (strlen(path) >= max_path_length) return -1;

    struct inode* dir_inode = inode_table_get(fs->it, directory->inode);
    if (dir_inode == NULL) return -1;
    if (dir_inode->type != INODE_DIRECTORY) return -1;


    struct directory_entry* dir_data_block = calloc(1, fs->sb->block_size);
    if (dir_data_block == NULL) {
        return -1;
    }


    size_t first_empty_block = (dir_inode->entries) / (fs->sb->block_size/sizeof(struct directory_entry));
    size_t first_empty_entry = (dir_inode->entries) % (fs->sb->block_size/sizeof(struct directory_entry));


    if (first_empty_block > dir_inode->blocks) {
        //if first_empty_block > dir_inode->blocks then something went very wrong

        free(dir_data_block);
        return -1;
    }

    if (first_empty_entry == 0 && first_empty_block == dir_inode->blocks) {
        //append block if the last block is full
        
        if (file_append_block(fs, directory) < 0) {
            free(dir_data_block);
            return -1;
        }
    }

    ///////

    if (file_read_block(fs, directory, dir_data_block, first_empty_block) < 0) {
        free(dir_data_block);
        return -1;
    }

    dir_data_block[first_empty_entry].inode = file->inode;
    memset(dir_data_block[first_empty_entry].name, 0, max_path_length);
    strcpy(dir_data_block[first_empty_entry].name, path);

    if (file_write_block(fs, directory, dir_data_block, first_empty_block) < 0) {
        free(dir_data_block);
        return -1;
    }

    dir_inode->entries++;
    if (inode_table_flush(fs) < FS_INODE_BLOCKS) {
        //ghost entry might be on disk
        free(dir_data_block);
        return -1;
    }


    free(dir_data_block);
    return 0;


}

int directory_remove(struct filesystem* fs, struct file* directory, char* path) {
    
    struct inode* dir_inode = inode_table_get(fs->it, directory->inode);
    if (dir_inode == NULL) return -1;
    if (dir_inode->type != INODE_DIRECTORY) return -1;
    if (dir_inode->blocks < 1) return -1;
    if (dir_inode->entries < 1) return -1;
    
    
    size_t last_block_index = (dir_inode->entries -1) / (fs->sb->block_size/sizeof(struct directory_entry));
    size_t last_entry_index = (dir_inode->entries -1) % (fs->sb->block_size/sizeof(struct directory_entry));


    size_t file_entry_index = 0;
    int res = directory_lookup(fs, directory, path, &file_entry_index);
    if (res < 0) return res; 
    size_t file_entry_block_index = file_entry_index / (fs->sb->block_size/sizeof(struct directory_entry));
    size_t file_entry_local_index = file_entry_index % (fs->sb->block_size/sizeof(struct directory_entry));

    struct directory_entry* last_block = calloc(1, fs->sb->block_size);
    if (last_block == NULL) return -1;
    struct directory_entry* file_entry_block = (last_block_index == file_entry_block_index)
    ? last_block
    : calloc(1, fs->sb->block_size);
    
    if (file_entry_block == NULL) {
        free(last_block);
        return -1;
    }

    if (file_read_block(fs, directory, last_block, last_block_index) < 0) {
        free(last_block);
        if (last_block != file_entry_block) free(file_entry_block);
        return -1;
    }

    if (last_block != file_entry_block 
        && file_read_block(fs, directory, file_entry_block, file_entry_block_index) < 0) {
            free(last_block);
            free(file_entry_block);
            return -1;    
        }

    file_entry_block[file_entry_local_index] = last_block[last_entry_index];

    if (file_write_block(fs, directory, file_entry_block, file_entry_block_index) < 0) {
        free(last_block);
        if (last_block != file_entry_block) free(file_entry_block);
        return -1;
    }

    dir_inode->entries--;

    if (inode_table_flush(fs) < FS_INODE_BLOCKS) {
        free(last_block);
        if (last_block != file_entry_block) free(file_entry_block);
        return -1;
    }

    free(last_block);
    if (last_block != file_entry_block) free(file_entry_block);

    return 0;

}

int directory_lookup(struct filesystem* fs, struct file* directory, char* path, size_t* out_index) {
    struct inode* dir_inode = inode_table_get(fs->it, directory->inode);
    if (dir_inode == NULL) return -1;
    if (dir_inode->type != INODE_DIRECTORY) return -1;

    if (dir_inode->entries == 0) return -1;

    size_t last_block_index = (dir_inode->entries -1) / (fs->sb->block_size/sizeof(struct directory_entry));
    size_t last_entry_index = (dir_inode->entries -1) % (fs->sb->block_size/sizeof(struct directory_entry));


    struct directory_entry* dir_data_block = calloc(1, fs->sb->block_size);
    if (dir_data_block == NULL) {
        return -1;
    }


    for (size_t i = 0; i <= last_block_index; i++) {

        if (file_read_block(fs, directory, dir_data_block, i) < 0) {
            free(dir_data_block);
            return -1;
        }

        size_t block_end = (i == last_block_index) 
        ? last_entry_index + 1
        : fs->sb->block_size / sizeof(struct directory_entry);

        for (size_t j = 0; j < block_end; j++) {
            if (!strcmp(path, dir_data_block[j].name)) {
                *out_index = i * (fs->sb->block_size / sizeof(struct directory_entry)) + j;
                free(dir_data_block);
                return 0;
            }
        }

    }

    //path doesnt exist
    free(dir_data_block);
    return -1;



}

struct file* directory_get_file(struct filesystem* fs, struct file* directory, char* path) {
    struct inode* dir_inode = inode_table_get(fs->it, directory->inode);
    if (dir_inode == NULL) return NULL;
    if (dir_inode->type != INODE_DIRECTORY) return NULL;

    if (dir_inode->entries == 0) return NULL;

    size_t file_index = 0;
    if (directory_lookup(fs, directory, path, &file_index) < 0) return NULL;

    struct directory_entry* dir_data_block = calloc(1, fs->sb->block_size);
    if (dir_data_block == NULL) return NULL;

    size_t block_index = file_index / (fs->sb->block_size/sizeof(struct directory_entry));
    size_t file_local_index = file_index % (fs->sb->block_size/sizeof(struct directory_entry));

    if (file_read_block(fs, directory, dir_data_block, block_index) < 0) {
        free(dir_data_block);
        return NULL;
    }

    struct file* file = calloc(1, sizeof(struct file));
    if (file == NULL) {
        free(dir_data_block);
        return NULL;
    }

    file->inode = dir_data_block[file_local_index].inode;
    
    free(dir_data_block);
    return file;

}
