#ifndef FS_INODE
#define FS_INODE

#include <stdint.h>
#include <sys/types.h>

struct inode {
    uint8_t type;
    size_t blocks;
    size_t index;
};


#endif