//superblock_read
//superblock_write
//superblock_validate

#include <stdlib.h>
#include <string.h>
#include "../include/superblock.h"
#include "../include/disk.h"
#include "../include/filesystem.h"

int superblock_read(struct disk* disk, struct superblock* superblock) {
    
    struct superblock sb = {0};

    if (disk_read(disk, &sb, 0, sizeof(sb)) != sizeof(sb)) {
        return -1;
    }

    *superblock = sb;

    return 0;
}

int superblock_write(struct filesystem* fs) {
    
    uint8_t block[fs->sb->block_size];
    memset(block, 0, sizeof(block));
    
    
    memcpy(block, fs->sb, sizeof(*fs->sb));
    
    int res = filesystem_flush_block(fs, block, 0);
    
    if (res < 0) return res;

    return 0;
}

int superblock_validate(const struct disk* disk, const struct superblock* superblock) {
    if (superblock->identifier != FS_IDENTIFIER) {
        return -1;
    }

    if (superblock->block_size == 0) {
        return -1;
    }
    if (superblock->block_count > disk->size / superblock->block_size) {
        return -1;
    }

    if (superblock->root_dir_index < 1 || superblock->root_dir_index > superblock->block_count) {
        return -1;
    }

    if (superblock->bitmap_index< 1 || superblock->bitmap_index > superblock->block_count) {
        return -1;
    }

    if (superblock->bitmap_index == superblock->root_dir_index) {
        return -1;
    }

    if (superblock->root_dir_index >= superblock->block_count) {
        return -1;
    }

    return 0;
}