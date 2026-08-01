
//#define __USE_UNIX98

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/disk.h"


// disk_open
// disk_close
// disk_read
// disk_write


struct disk* disk_open(const char* path) {
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("open");
        return NULL;
    }

    struct stat diskstat;
    if (fstat(fd, &diskstat) ==-1) {
        perror("fstat");
        if (close(fd) == -1) {
            perror("close");
        }
        return NULL;
    }

    struct disk* disk = malloc(sizeof(*disk));
    if (disk == NULL) {
        perror("malloc");
        if (close(fd) == -1) {
            perror("close");
        }
        return NULL;
    }


    disk->fd = fd;
    disk->size = diskstat.st_size;

    return disk;
}

void disk_close(struct disk* disk) {
    if (close(disk->fd) == -1) {
        perror("close");
    }
    free(disk);
    return;
}

ssize_t disk_read(struct disk* disk, void* buffer, off_t offset, size_t count) {
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


