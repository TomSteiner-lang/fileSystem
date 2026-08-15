
#include <stdlib.h>
#include <string.h>

#include "../include/inode_table.h"
#include "../include/inode.h"
#include "../include/superblock.h"
#include "../include/filesystem.h"

//inode_table_create
//inode_table_destroy
//inode_table_load
//inode_table_flush
//inode_table_add
//inode_table_remove

size_t inode_table_add(struct inode_table* it, int type) {
    //returns the number of the new inode and sets its type
    
    //zero is reserved for the root dir inode and is used here to signify failure
    if (type == INODE_FREE) return 0;

    
    for (size_t i = 1; i < it->max_inodes; i++) {
        if (it->inodes[i].type == INODE_FREE) {
            it->inodes[i].type = type;
            return i;
        }
    }
        
    return 0;

}

void inode_table_remove(struct inode_table* it, size_t index) {
    
    if (index >= it->max_inodes) return;
    
    it->inodes[index].type = INODE_FREE;
    return;
}

int inode_table_create(const struct superblock* sb, struct inode_table* it) {
    
    struct inode* inodes = malloc((sb->inode_table_size * sb->block_size));
    if (inodes == NULL) {
        return -1;
    }

    memset(inodes, 0, sb->inode_table_size * sb->block_size);

    it->inodes = inodes;
    size_t max_inodes = (sb->inode_table_size * sb->block_size) / sizeof(struct inode);
    it->max_inodes = max_inodes;
   

    return 0;
}

void inode_table_destroy(struct inode_table* it) {
    free(it->inodes);
    it->inodes = NULL;
    it->max_inodes = 0;
    return;
}


int inode_table_load(struct filesystem* fs) {

    uint8_t* inodes = malloc(fs->sb->inode_table_size * fs->sb->block_size);
    if (inodes == NULL) {
        return -1;
    }

    for (size_t i = 0; i < fs->sb->inode_table_size; i++) {
        int res = filesystem_load_block(fs,
            inodes + i * fs->sb->block_size,
            i + fs->sb->inode_table_index);
        
        if (res < 0) {
            free(inodes);
            return -1;
        } 
    }

    size_t max_inodes = (fs->sb->inode_table_size * fs->sb->block_size) / sizeof(struct inode);
    fs->it->inodes = (struct inode*) inodes;
    fs->it->max_inodes = max_inodes;
    return 0;    

}

size_t inode_table_flush(struct filesystem* fs) {

    for (size_t i = 0; i < fs->sb->inode_table_size; i++) {
        int res = filesystem_flush_block(fs,
        (uint8_t *)fs->it->inodes + i * fs->sb->block_size,
        fs->sb->inode_table_index + i);

        if (res < 0) return i;
    }

    return fs->sb->inode_table_size;
}