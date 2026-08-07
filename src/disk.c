
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/disk.h"

// disk_create
// disk_open
// disk_close
// disk_read
// disk_write

int disk_create(const char* path, off_t size) {
    
    int fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0644);

    if (fd == -1) {
        return -1;
    }
    if (ftruncate(fd, size) == -1) {
        close(fd);
        return -1;
    }

    if (close(fd) == -1) {
        return -1;
    }


    

    return 0;
}

struct disk* disk_open(const char* path) {
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        return NULL;
    }

    struct stat diskstat;
    if (fstat(fd, &diskstat) ==-1) {
        close(fd);
        return NULL;
    }

    struct disk* disk = malloc(sizeof(*disk));
    if (disk == NULL) {
        close(fd);
        return NULL;
    }


    disk->fd = fd;
    disk->size = diskstat.st_size;

    return disk;
}

int disk_close(struct disk* disk) {
    if (close(disk->fd) == -1) {
        return -1;
    }
    free(disk);
    return 0;
}

ssize_t disk_read(const struct disk* disk, void* buffer, off_t offset, size_t count) {
    if (offset < 0) {
        return -1;
    }

    if (offset > disk->size) {
        return -1;
    }

    if (count > disk->size - (uint64_t)offset) {
        return -1;
    }

    
    ssize_t n = pread(disk->fd, buffer, count, offset);


    return n;
}

ssize_t disk_write(struct disk* disk, const void* buffer, off_t offset, size_t count) {
    if (offset < 0) {
        return -1;
    }

    if (offset > disk->size) {
        return -1;
    }

    if (count > disk->size - (uint64_t)offset) {
        return -1;
    }



    ssize_t n = pwrite(disk->fd, buffer, count, offset);


    return n;
}


