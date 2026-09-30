#ifndef FS_TRANSACTION
#define FS_TRANSACTION

#include <sys/types.h>




enum transaction_status {
    TRANSACTION_ACTIVE = 1,
    TRANSACTION_CLEANUP_RESOURCES = 2,
    TRANSACTION_ALLOCATING = 3,
    TRANSACTION_DEAD = 0
};


struct transaction_table {
    size_t active_transactions;
    size_t* free_slots;
    size_t free_slots_count;
};

struct transaction {
    size_t table_index;
    size_t reserved_block;    
    int status;
};

struct transaction_handle {
    struct transaction transaction;
    size_t disk_index;
    size_t entry_count;
};

struct transaction_entry {
    size_t block_number;
    size_t backup_index;
    int status;
};


int transaction_table_create(struct superblock* sb, struct transaction_table* tt);
void transaction_table_destroy(struct transaction_table* tt);
struct transaction_handle* transaction_create(struct filesystem* fs);
int transaction_add_blocks(struct filesystem* fs, struct transaction_handle* t, size_t block_count, size_t* blocks);
int transaction_commit(struct filesystem* fs, struct transaction_handle* t);
int transaction_abort(struct filesystem* fs, struct transaction_handle* t);






#endif