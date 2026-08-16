#ifndef FS_FILESYSTEM
#define FS_FILESYSTEM

#define FS_MIN_SIZE 1024 * 128
#define FS_MIN_BLOCKS 128
#define FS_INODE_BLOCKS 10


#include "./disk.h"
#include "./superblock.h"
#include "./bitmap.h"
#include "./inode_table.h"

struct filesystem {
    struct disk* disk;
    struct superblock* sb;
    struct bitmap* bm;
    struct inode_table* it;
};
struct file* filesystem_create_file(struct filesystem* fs, int type);
int filesystem_delete_file(struct filesystem* fs, struct file* file);
int filesystem_flush_block(struct filesystem* fs,const void* block ,size_t block_number);
int filesystem_load_block(struct filesystem* fs,void* block ,size_t block_number);
int filesystem_unmount(struct filesystem* fs);
struct filesystem* filesystem_mount(struct disk* disk);
int filesystem_create(struct disk* disk, size_t block_size);


#endif