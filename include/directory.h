#ifndef FS_DIRECTORY
#define FS_DIRECTORY

#include <sys/types.h>

struct directory_entry {
    size_t inode;
    char name[128];
};

struct file;
struct filesystem;

int directory_add(struct filesystem* fs, struct file* directory, struct file* file, char* path);
int directory_remove(struct filesystem* fs, struct file* directory, char* path);
int directory_lookup(struct filesystem* fs, struct file* directory, char* path, size_t* out_index);
struct file* directory_get_file(struct filesystem* fs, struct file* directory, char* path);


#endif