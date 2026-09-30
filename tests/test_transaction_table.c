
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include "../include/filesystem.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_PATH "./images/test.img"
#define TEST_DISK_SIZE 1024 * 1024 * 1
#define TEST_BLOCK_SIZE 1024

int main(void) {
    unlink(TEST_PATH);

    off_t size = TEST_DISK_SIZE;
    int created = disk_create(TEST_PATH, size);
    assert(!created);
    struct disk * disk = disk_open(TEST_PATH);
    assert(filesystem_create(disk, TEST_BLOCK_SIZE) == 0);
    struct filesystem* fs = filesystem_mount(disk);






    assert(fs->tt->free_slots != NULL);
    assert(fs->tt->free_slots_count == 0);
    assert(fs->tt->active_transactions == 0);

    struct transaction_handle* t = transaction_create(fs);

    assert(t != NULL);
    assert(fs->tt->active_transactions == 1);
    assert(t->disk_index == 0);
    assert(t->entry_count == 0);
    assert(t->transaction.status==TRANSACTION_ACTIVE);
    assert(t->transaction.reserved_block == t->transaction.table_index);
    assert(bitmap_is_set(fs->bm, t->transaction.table_index));


    struct transaction_handle* t2 = transaction_create(fs);
    assert(t2 != NULL);
    assert(fs->tt->active_transactions == 2);
    assert(t2->disk_index == 1);
    assert(t2->entry_count == 0);
    assert(t2->transaction.status==TRANSACTION_ACTIVE);
    assert(t2->transaction.reserved_block == t2->transaction.table_index);
    assert(bitmap_is_set(fs->bm, t2->transaction.table_index));



    char* testblock = calloc(1, fs->sb->block_size);
    assert(testblock != NULL);

    char testmessage[] = "this is a test\n";
    memcpy(testblock, testmessage, sizeof(testmessage));

    size_t blocks[10];

    for (int i = 0; i < 10; i++) {
        assert(bitmap_allocate(fs->bm, blocks+i) == 0);
        assert(filesystem_flush_block(fs, testblock, blocks[i]) == 0);
    }






    int added_blocks = transaction_add_blocks(fs, t, 10, blocks);
    assert(added_blocks == 0);
    assert(t->entry_count == 10);
    
    struct transaction_entry* transaction_index = calloc(1, fs->sb->block_size);
    assert(transaction_index != NULL);


    assert(filesystem_load_block(fs, transaction_index, t->transaction.table_index) == 0);


    for (size_t i = 0; i < t->entry_count; i++) {
        assert(transaction_index[i].block_number == blocks[i]);
        assert(transaction_index[i].status == TRANSACTION_ACTIVE);
        memset(testblock, 0, fs->sb->block_size);
        assert(filesystem_load_block(fs, testblock, transaction_index[i].backup_index) == 0);
        assert(!strcmp(testblock, testmessage));
    }



    assert(t->disk_index + fs->sb->transaction_table_index == 15);

    assert(transaction_commit(fs, t) == 0);

    
    assert(t->transaction.status == TRANSACTION_DEAD);
    assert(filesystem_load_block(fs, testblock, t->disk_index + fs->sb->transaction_table_index) == 0);
    assert(((struct transaction*) testblock)[0].status == TRANSACTION_DEAD);

    assert(fs->tt->free_slots_count == 1);
    assert(fs->tt->free_slots[fs->tt->free_slots_count-1] == t->disk_index);

    free(t);


    for (int i = 0; i < 10; i++) {
        assert(filesystem_load_block(fs, testblock, transaction_index[i].backup_index) == 0);
        assert(!strcmp(testblock, testmessage));
    }

    char message2[] = "this should get rolled back\n";

    assert(transaction_add_blocks(fs, t2, 10, blocks) == 0);
    memset(testblock, 0, fs->sb->block_size);
    strcpy(testblock, message2);

    for (int i = 0; i < 10; i++) {
        assert(filesystem_flush_block(fs, testblock, blocks[i]) == 0);
    }

    assert(transaction_abort(fs, t2) == 0);

    for (int i = 0; i < 10; i++) {
        assert(filesystem_load_block(fs, testblock, transaction_index[i].backup_index) == 0);
        assert(!strcmp(testblock, testmessage));
    }

    

    assert(!filesystem_unmount(fs));
    assert(!disk_close(disk));
    assert(!unlink(TEST_PATH));
    
    return 0;

}