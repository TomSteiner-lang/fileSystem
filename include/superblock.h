#ifndef FS_SUPERBLOCK
#define FS_SUPERBLOCK

#include <stdint.h>
#include "disk.h"

#define FS_IDENTIFIER 9299611

struct superblock {
    uint64_t identifier;
    uint64_t block_size;
    uint64_t block_count;
    uint64_t root_dir_index;
    uint64_t bitmap_index;
};

int superblock_read(struct disk* disk, struct superblock* superblock);
int superblock_write(struct disk* disk, const struct superblock* superblock);
int superblock_validate(const struct disk* disk, const struct superblock* superblock);



#endif