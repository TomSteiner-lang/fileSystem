#ifndef DISK
#define DISK

#include <sys/stat.h>
#include <sys/types.h>

struct disk {
    int fd;
    off_t size;
};

struct disk* disk_open(const char* path);
void disk_close(struct disk* disk);
ssize_t disk_read(struct disk* disk, void* buffer, off_t offset, size_t count);
ssize_t disk_write(struct disk* disk, const void* buffer, off_t offset, size_t count);




#endif