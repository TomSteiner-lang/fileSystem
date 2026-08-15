#ifndef FS_SUPERBLOCK
#define FS_SUPERBLOCK

#include <stdint.h>
#include "disk.h"


#define FS_IDENTIFIER 9299611

struct filesystem;

struct superblock {
    uint64_t identifier;
    uint64_t block_size;
    size_t block_count;
    uint64_t root_dir_index;
    uint64_t bitmap_index;
    uint64_t inode_table_index;
    size_t inode_table_size;
};

int superblock_read(struct disk* disk, struct superblock* superblock);
int superblock_write(struct filesystem* fs);
int superblock_validate(const struct disk* disk, const struct superblock* superblock);



#endif