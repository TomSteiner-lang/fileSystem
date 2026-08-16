#ifndef FS_INODE_TABLE
#define FS_INODE_TABLE


#include <sys/types.h>
#include "./inode.h"
#include "./superblock.h"

struct filesystem;
struct inode_table {
    size_t max_inodes;
    struct inode* inodes;
};

int inode_table_set(struct inode_table* it, struct inode* inode, size_t index);
struct inode* inode_table_get(struct inode_table* it, size_t index);
size_t inode_table_add(struct inode_table* it, int type);
void inode_table_remove(struct inode_table* it, size_t index);
int inode_table_create(const struct superblock* sb, struct inode_table* it);
void inode_table_destroy(struct inode_table* it);
int inode_table_load(struct filesystem* fs);
size_t inode_table_flush(struct filesystem* fs);





#endif