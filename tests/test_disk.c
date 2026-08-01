#include <string.h>
#include <assert.h>
#include "../include/disk.h"


int main(void) {
    
    struct disk * disk = disk_open("./images/disk.img");

    char buff[] = "test\n";

    disk_write(disk, buff, 0, sizeof(buff));


    char in[10];

    disk_read(disk, in, 0, sizeof(buff));

    assert(!strcmp("test\n", in));

    char erase[] = "\0\0\0\0\0\0\0\0\0\0\0\0";

    disk_write(disk, erase, 0, sizeof(erase));


    disk_close(disk);
    
    return 0;
}
