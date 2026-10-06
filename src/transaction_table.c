

#include <stdlib.h>
#include <string.h>

#include "../include/filesystem.h"
#include "../include/transaction.h"


// transaction_table_cleanup
// transaction_create
// transaction_backup_block
// transaction_rollback
// transaction_table_get (static)

// on any transaction function fail cleanup must be initiated

// currently not handling 



int transaction_table_create(struct superblock* sb, struct transaction_table* tt) {

    //will be dynamic after rehaul 
    size_t max_transactions = sb->transaction_table_size * (sb->block_size / sizeof(struct transaction));
    size_t* free_slots = calloc(max_transactions, sizeof(size_t));
    if (free_slots == NULL) {
        return -1;
    }

    tt->free_slots=free_slots;
    return 0;

}

void transaction_table_destroy(struct transaction_table* tt) {
    if (tt->free_slots != NULL) free(tt->free_slots);
    tt->active_transactions = 0;
    tt->free_slots = NULL;
    tt->free_slots_count = 0;
    return;
}

struct transaction_handle* transaction_create(struct filesystem* fs) {
    size_t new_transaction_index = (fs->tt->free_slots_count > 0)
    ? fs->tt->free_slots[fs->tt->free_slots_count - 1]
    : fs->tt->active_transactions;

    size_t new_transaction_block_index = new_transaction_index 
    / (fs->sb->block_size / sizeof(struct transaction));
    new_transaction_block_index += fs->sb->transaction_table_index;


    size_t new_transaction_local_index = new_transaction_index 
    % (fs->sb->block_size / sizeof(struct transaction));
    
    
    struct transaction* new_transaction_block = calloc(1, fs->sb->block_size);
    if (new_transaction_block == NULL) {
        return NULL;
    }

    struct transaction_handle* ret = calloc(1, sizeof(struct transaction_handle));
    if (ret == NULL) {
        free(new_transaction_block);
        return NULL;
    }

    struct transaction_entry* transaction_entry_table_block = calloc(1, fs->sb->block_size);

    if (transaction_entry_table_block == NULL) {
        free(ret);
        free(new_transaction_block);
        return NULL;
    }



    if (filesystem_load_block(fs, new_transaction_block, 
        new_transaction_block_index) < 0) {
            free(new_transaction_block);
            free(ret);
            free(transaction_entry_table_block);
            return NULL;
        }



    if (new_transaction_block[new_transaction_local_index].status != TRANSACTION_DEAD) {
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);
        return NULL;
    }


    new_transaction_block[new_transaction_local_index].status = TRANSACTION_ALLOCATING;

    size_t reserved_block = 0;



    if (bitmap_reserve(fs->bm, &reserved_block) < 0) {
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);
        return NULL;
    }

    new_transaction_block[new_transaction_local_index].reserved_block = reserved_block;

    new_transaction_block[new_transaction_local_index].table_index = reserved_block;






    if (filesystem_flush_block(fs, new_transaction_block, 
        new_transaction_block_index) < 0) {
            bitmap_release_reserve(fs->bm, new_transaction_block[new_transaction_local_index].table_index);
            free(new_transaction_block);
            free(ret);
            free(transaction_entry_table_block);
            return NULL;
        }


        

    if (bitmap_set(fs->bm, new_transaction_block[new_transaction_local_index].table_index) < 0) {
        bitmap_release_reserve(fs->bm, new_transaction_block[new_transaction_local_index].table_index);
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);
        return NULL;
    }


    int res = bitmap_flush(fs);
    
    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }
    
    if (res == BITMAP_FAIL) {
        bitmap_free(fs->bm, new_transaction_block[new_transaction_local_index].table_index);
        bitmap_release_reserve(fs->bm, new_transaction_block[new_transaction_local_index].table_index);
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);
        return NULL;

    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic failure recovery
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);

        return NULL;
    }

    int cleared = filesystem_flush_block(fs,
        transaction_entry_table_block,
        new_transaction_block[new_transaction_local_index].table_index);
    if (cleared < 0) {
        free(new_transaction_block);
        free(ret);
        free(transaction_entry_table_block);
        return NULL;
    }


    new_transaction_block[new_transaction_local_index].status = TRANSACTION_ACTIVE;



    res = filesystem_flush_block(fs, new_transaction_block, 
        new_transaction_block_index);
        
    if (res < 0) {
        if (filesystem_load_block(fs,
            new_transaction_block,
            new_transaction_block_index ) < 0) {
            // todo - handle this specifically. we dont know if the transaction persisted, if it didnt everything is fine but if it did we need a way to clean it up
            
            free(new_transaction_block);
            free(ret);
            free(transaction_entry_table_block);

            return NULL;
        }

        if (new_transaction_block[new_transaction_local_index].status != TRANSACTION_ACTIVE) {
            free(new_transaction_block);
            free(ret);
            free(transaction_entry_table_block);

            return NULL;
        }
    }



    if (fs->tt->free_slots_count > 0) fs->tt->free_slots_count--;
    else fs->tt->active_transactions++;

    ret->entry_count = 0;
    ret->disk_index = new_transaction_index;
    ret->transaction.reserved_block = new_transaction_block[new_transaction_local_index].reserved_block;
    ret->transaction.status = TRANSACTION_ACTIVE;
    ret->transaction.table_index = new_transaction_block[new_transaction_local_index].table_index;

    return ret;




}


static void transaction_table_release_slot(struct transaction_table* tt, size_t index) {
        
    if (tt->active_transactions - index == 1) {
        tt->active_transactions--;
        return;
    }

    for (size_t i =tt->free_slots_count; i-- > 0;) {
        if (tt->free_slots[i] == index) return;        
    }


    tt->free_slots[tt->free_slots_count] = index;
    tt->free_slots_count++;
    return;


}

static int transaction_cleanup_resources(struct filesystem* fs, struct transaction_handle* t) {
    struct transaction_entry* transaction_entry_table_block = calloc(2, fs->sb->block_size);
    if (transaction_entry_table_block == NULL) return -1;

    struct transaction* transaction_table_block = 
    (struct transaction* )((uint8_t*) transaction_entry_table_block + fs->sb->block_size);



    size_t transaction_block_index = t->disk_index 
    / (fs->sb->block_size / sizeof(struct transaction))
    + fs->sb->transaction_table_index;
    size_t transaction_local_index = t->disk_index 
    % (fs->sb->block_size / sizeof(struct transaction));



    if (filesystem_load_block(fs, transaction_table_block, transaction_block_index) < 0) {
        free(transaction_entry_table_block);
        return -1;
    }




    if (transaction_table_block[transaction_local_index].status == TRANSACTION_DEAD) {

        transaction_table_release_slot(fs->tt, t->disk_index);
        free(transaction_entry_table_block);
        return 0;

    }

        


    if (transaction_table_block[transaction_local_index].status != TRANSACTION_ACTIVE 
    && transaction_table_block[transaction_local_index].status != TRANSACTION_CLEANUP_RESOURCES ) {
        //shouldnt happen, this means something is corrupted
        free(transaction_entry_table_block);
        
        return -1;
    }


    transaction_table_block[transaction_local_index].status = TRANSACTION_CLEANUP_RESOURCES;
    if (filesystem_flush_block(fs, transaction_table_block, transaction_block_index) < 0) {
        free(transaction_entry_table_block);
        return -1;
    }
    t->transaction.status = TRANSACTION_CLEANUP_RESOURCES;

    if (filesystem_load_block(fs, transaction_entry_table_block, t->transaction.table_index) < 0) {
        free(transaction_entry_table_block);
        return -2;
    }



    for (size_t i = 0; i < t->entry_count; i++) {
        size_t backup_index = transaction_entry_table_block[i].backup_index;
        if (bitmap_is_set(fs->bm, backup_index)) {
            bitmap_free(fs->bm, backup_index);
        }
        if (bitmap_is_reserved(fs->bm, backup_index)) {
            //dont know if this is necessary
            bitmap_release_reserve(fs->bm, backup_index);
        }
    }

    if (bitmap_is_set(fs->bm, t->transaction.table_index)) {
        bitmap_free(fs->bm, t->transaction.table_index);
    }

    if (bitmap_is_reserved(fs->bm, t->transaction.table_index)) {
        bitmap_release_reserve(fs->bm, t->transaction.table_index);
    }
    

    int res = bitmap_flush(fs);
    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        free(transaction_entry_table_block);
        return -2;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic faliure recovery
        free(transaction_entry_table_block);
        return -2;
    }

    transaction_table_block[transaction_local_index].status = TRANSACTION_DEAD;
    if (filesystem_flush_block(fs, transaction_table_block, transaction_block_index) < 0) {
        free(transaction_entry_table_block);
        return -2;
    }
    t->transaction.status = TRANSACTION_DEAD;

    transaction_table_release_slot(fs->tt, t->disk_index);

    free(transaction_entry_table_block);
    return 0;

}


int transaction_add_blocks(struct filesystem* fs, struct transaction_handle* t, size_t block_count, size_t* blocks) {
    
    // todo - return enum that signals whether recovery is needed

    if (block_count < 1) return -1;
    if (blocks == NULL) return -1;


    if (t->entry_count + block_count
        > (fs->sb->block_size / sizeof(struct transaction_entry))) {
            //keep this until arbitrary sized transactions and files
            return -1;
        }

    struct transaction_entry* entry_table = calloc(1, fs->sb->block_size);
    if (entry_table == NULL) return -1;

    uint8_t* data_buffer = calloc(1, fs->sb->block_size);
    if (data_buffer == NULL) {
        free(entry_table);
        return -1;
    }


    size_t initial_entry_count = t->entry_count;

    size_t reserved[block_count];
    memset(reserved, 0, sizeof(reserved));


    if (filesystem_load_block(fs, entry_table, t->transaction.table_index) < 0) {
        free(entry_table);
        free(data_buffer);
        return -1;
    }

    for (size_t i = 0; i < block_count; i++) {
        if (bitmap_reserve(fs->bm, &reserved[i]) < 0) {
            for (size_t j = 0; j < i; j++) {
                bitmap_release_reserve(fs->bm, reserved[j]);
            }
            free(entry_table);
            free(data_buffer);
            return -1;
        }
        entry_table[initial_entry_count + i].status = TRANSACTION_ALLOCATING;
        entry_table[initial_entry_count + i].block_number = blocks[i];
        entry_table[initial_entry_count + i].backup_index = reserved[i];
    }

    t->entry_count += block_count;

    if (filesystem_flush_block(fs, entry_table, t->transaction.table_index) < 0) {
        free(entry_table);
        free(data_buffer);
        return -1;
    }





    for (size_t i = 0; i < block_count; i++) {
        if (bitmap_set(fs->bm, reserved[i]) < 0) {
            for(size_t j = 0; j < i; j++) {
                bitmap_free(fs->bm, reserved[j]);
            }

            
            free(entry_table);
            free(data_buffer);
            return -1;
        }
    }


    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        for (size_t i = 0; i < block_count; i++) {
            bitmap_free(fs->bm, reserved[i]);
        }
        free(entry_table);
        free(data_buffer);
        return -1;
    }

    if (res == BITMAP_INDETERMINATE) {
        // todo - catastrophic failure recovery
        free(entry_table);
        free(data_buffer);
        return -1;
    }



    for (size_t i = 0; i < block_count; i++) {
        if (filesystem_load_block(fs, data_buffer, blocks[i]) < 0) {
            free(entry_table);
            free(data_buffer);
            return -1;
        }

        if (filesystem_flush_block(fs, data_buffer, reserved[i]) < 0) {
            free(entry_table);
            free(data_buffer);
            return -1;
        }
    }


    for (size_t i = 0; i < block_count; i++) {
        entry_table[initial_entry_count+ i].status = TRANSACTION_ACTIVE;
    }


    if (filesystem_flush_block(fs, entry_table, t->transaction.table_index) < 0) {
        free(entry_table);
        free(data_buffer);
        return -1;
    }
    

    return 0;
    
}




int transaction_commit(struct filesystem* fs, struct transaction_handle* t) {
    // 0 means committed
    // -2 means data committed but cleanup failed
    int ret = transaction_cleanup_resources(fs, t);
    if (ret == 0 || ret == 2) return 0;
    return ret;
}

int transaction_abort(struct filesystem* fs, struct transaction_handle* t) {
    //this function should be called whenever a transaction was made and something wrong happened in order to undo what the transaction backed up

    
    uint8_t* blocks = calloc(3, fs->sb->block_size);
    if (blocks == NULL) return -1;
    struct transaction* transaction_table = (struct transaction*) blocks;
    struct transaction_entry* entry_table = (struct transaction_entry*) (blocks + fs->sb->block_size);
    uint8_t* data_buffer = blocks + (2 * fs->sb->block_size);

    size_t transaction_table_block_index = t->disk_index 
    / (fs->sb->block_size / sizeof(struct transaction))
    + fs->sb->transaction_table_index;



    size_t transaction_local_index = t->disk_index 
    % (fs->sb->block_size / sizeof(struct transaction));



    if (filesystem_load_block(fs, transaction_table, transaction_table_block_index) < 0) {
        free(blocks);
        return -1;
    }

    if (transaction_table[transaction_local_index].status == TRANSACTION_CLEANUP_RESOURCES) {
        free(blocks);
        return (transaction_cleanup_resources(fs, t));
    }

    if (transaction_table[transaction_local_index].status != TRANSACTION_ACTIVE) {
        free(blocks);
        return -1;
    }


    
    if (filesystem_load_block(fs, entry_table, t->transaction.table_index) < 0) {
        free(blocks);
        return -1;
    }

    for(size_t i = 0; i < t->entry_count; i++) {
        if (filesystem_load_block(fs, data_buffer, entry_table[i].backup_index) < 0) {
            free(blocks);
            return -1;
        }

        if (filesystem_flush_block(fs, data_buffer, entry_table[i].block_number) < 0) {
            free(blocks);
            return -1;
        }
    }

    if (transaction_cleanup_resources(fs, t) < 0) {
        free(blocks);
        return -1;
    }

    free(blocks);

    return 0;

}