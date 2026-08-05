#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/disk.h"

#define TEST_PATH "./images/test.img"
#define TEST_DISK_SIZE

int main(void) {
    unlink(TEST_PATH);

    off_t size = 1024 * 1024 * 1;

    int created = disk_create(TEST_PATH, size);

    assert(!created);
    struct disk * disk = disk_open(TEST_PATH);

    char buff[] = "test\n";

    disk_write(disk, buff, 0, sizeof(buff));


    char in[10];

    disk_read(disk, in, 0, sizeof(buff));

    assert(!strcmp("test\n", in));

    char erase[] = "\0\0\0\0\0\0\0\0\0\0\0\0";

    disk_write(disk, erase, 0, sizeof(erase));


    assert(!disk_close(disk));

    assert(!unlink(TEST_PATH));
    
    return 0;
}
