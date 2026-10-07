#ifndef FS_FILE
#define FS_FILE


#include <sys/types.h>
#include <stdint.h>

struct filesystem;
struct file {
    size_t inode;
};


// NOT RELATED TO inode.entries
struct file_block_entry {
    size_t block_number;
    uint8_t status;
};

enum file_block_entry_status {
    FILE_BLOCK_FREE = 0,
    FILE_BLOCK_ALLOCATING = 1,
    FILE_BLOCK_ALLOCATED = 2,
    FILE_BLOCK_DEALLOCATING = 3
};

int file_write_block(struct filesystem* fs, struct file* file, void* buff, size_t index);
int file_read_block(struct filesystem* fs, struct file* file, void* buff, size_t index);
int file_append_block(struct filesystem* fs, struct file* file);
int file_pop_block(struct filesystem* fs, struct file* file);






#endif