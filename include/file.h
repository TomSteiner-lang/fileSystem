#ifndef FS_FILE
#define FS_FILE


#include <sys/types.h>

struct filesystem;
struct file {
    size_t inode;
};

int file_write_block(struct filesystem* fs, struct file* file, void* buff, size_t index);
int file_read_block(struct filesystem* fs, struct file* file, void* buff, size_t index);
int file_append_block(struct filesystem* fs, struct file* file);
int file_pop_block(struct filesystem* fs, struct file* file);






#endif