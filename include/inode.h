#ifndef FS_INODE
#define FS_INODE

#include <stdint.h>
#include <sys/types.h>

enum inode_type {
    INODE_FREE = 0,
    INODE_FILE = 1,
    INODE_DIRECTORY = 2,
};

struct inode {
    uint8_t type;
    size_t blocks;
    size_t index;
};


#endif