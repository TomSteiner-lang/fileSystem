#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/filesystem.h"
#include "../include/file.h"
#include "../include/inode.h"

//file_append_block
//file_pop_block
//file_write_block
//file_read_block


int file_write_block(struct filesystem* fs, struct file* file, void* buff, size_t index) {

    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if (index >= inode->blocks) return -1;

    struct file_block_entry* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(index_block);
        return -1;
    }

    if (filesystem_flush_block(fs, buff, index_block[index].block_number) < 0) {
        //disk corrupted
        free(index_block);
        return -1;
    }

    free(index_block);
    return 0;
}

int file_read_block(struct filesystem* fs, struct file* file, void* buff, size_t index) {
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;

    if (index >= inode->blocks) return -1;

    struct file_block_entry* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    void* temp_buff = calloc(1, fs->sb->block_size);
    if (temp_buff == NULL) {
        free(index_block);
        return -1;
    }

    if (filesystem_load_block(fs, index_block, inode->index) < 0) {
        free(temp_buff);
        free(index_block);
        return -1;
    }

    if (filesystem_load_block(fs, temp_buff, index_block[index].block_number) < 0) {
        free(temp_buff);
        free(index_block);
        return -1;
    }

    memcpy(buff, temp_buff, fs->sb->block_size);

    free(index_block);
    free(temp_buff);

    return 0;
}

int file_append_block(struct filesystem* fs, struct file* file) {
    
    //todo - set status flag in inode to persist that a change is in progress 

    //mechanism:
    //get inode
    //reserve block - fail: nothing
    //mark block allocation intent - fail: unreserve block
    //blocks++ - fail: us nothing, call recovery
    //allocate block - fail: as above
    //mark block as allocated - fail: as above

    // 3 big recovery paths: file, transaction, bitmap
    
    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;
    struct inode inode_copy = *inode;

    if((inode_copy.blocks + 1) * sizeof(struct file_block_entry) > fs->sb->block_size) {
        return -1;
    }




    struct file_block_entry* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) {
        return -1;
    }

    if (filesystem_load_block(fs, index_block, inode_copy.index) < 0) {
        free(index_block);
        return -1;
    }


    size_t newblock = 0;

    if (bitmap_reserve(fs->bm, &newblock) < 0) {
        free(index_block);
        return -1;
    }

    index_block[inode_copy.blocks].block_number = newblock;
    index_block[inode_copy.blocks].status = FILE_BLOCK_ALLOCATING;




    struct transaction_handle* t = transaction_create(fs);

    if (t == NULL) {
        //call recovery
        bitmap_release_reserve(fs->bm, newblock);
        free(index_block);
        return -1;
    }

    if (transaction_add_blocks(fs, t, 1, &inode_copy.index) < 0) {
        bitmap_release_reserve(fs->bm, newblock);
        free(index_block);
        if (transaction_abort(fs ,t) < 0) {
            //call recovery
        }
        free(t);
        return -1;
    }

    if (filesystem_flush_block(fs, index_block, inode_copy.index) < 0) {
        bitmap_release_reserve(fs->bm, newblock);
        free(index_block);
        int res = transaction_abort(fs, t);
        if (res < 0) {
            //call recovery
        }
        free(t);
        return -1;
    }


    if (transaction_commit(fs, t) < 0) {
        bitmap_release_reserve(fs->bm, newblock);
        free(index_block);
        int res = transaction_abort(fs, t);
        if (res < 0) {
            //call recovery
        }
        free(t);
        return -1;
    }
    free(t);

    inode_copy.blocks++;
    if (inode_table_set(fs->it, &inode_copy, file->inode) < 0) {
        // call recovery
        free(index_block);
        return -1;
    }

    if (inode_table_flush(fs) < 0) {
        // call recovery
        free(index_block);
        return -1;
    }


    if (bitmap_set(fs->bm, newblock) < 0) {
        //call recovery
        free(index_block);
        return -1;
    }

    int res = bitmap_flush(fs);

    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }


    if (res == BITMAP_FAIL) {
        //call recovery
        free(index_block);
        return -1;
    }


    if (res == BITMAP_INDETERMINATE) {
        //call bitmap and normal recovery
        free(index_block);
        return -1;
    }

    index_block[inode_copy.blocks -1].status = FILE_BLOCK_ALLOCATED;

    t = transaction_create(fs);
    if (t == NULL) {
        //call file and transaction recovery
        free(index_block);
        return -1;
    }

    if (transaction_add_blocks(fs ,t, 1, &inode_copy.index) < 0) {
        free(index_block);
        //call file recovery
        if (transaction_abort(fs ,t) < 0) {
            //call transaction recovery
        }
        free(t);
        return -1;
    }
    
    if (filesystem_flush_block(fs, index_block, inode_copy.index) < 0) {
        free(index_block);
        //call file recovery
        if (transaction_abort(fs ,t) < 0) {
            //call transaction recovery
        }
        free(t);
        return -1;
    }

    if (transaction_commit(fs, t) < 0) {
        free(index_block);
        //call file recovery
        if (transaction_abort(fs ,t) < 0) {
            //call transaction recovery
        }
        free(t);
        return -1;
    }


    free(t);
    free(index_block);   

    return 0;


}

int file_pop_block(struct filesystem* fs, struct file* file) {

    //todo - add inode status



    //mechanism:
    //get inode - fail: nothing
    //mark deallocation intent - fail: nothing
    //release block + reserve it - fail: call recovery
    //decrement counter - fail: call recovery
    //release reserve - cant fail
    //mark status as free - fail: call recovery



    struct inode* inode = inode_table_get(fs->it, file->inode);
    if (inode == NULL) return -1;
    struct inode inode_copy = *inode;


    if (inode_copy.blocks == 0) return -1;

    struct file_block_entry* index_block = calloc(1, fs->sb->block_size);
    if (index_block == NULL) return -1;

    if (filesystem_load_block(fs, index_block, inode_copy.index) < 0) {
        free(index_block);
        return -1;
    }

    index_block[inode_copy.blocks-1].status = FILE_BLOCK_DEALLOCATING;

    struct transaction_handle* t = transaction_create(fs);
    if (t == NULL) {
        //call transaction recovery
        free(index_block);
        return -1;
    }


    if (transaction_add_blocks(fs, t, 1, &inode_copy.index) < 0) {
        if (transaction_abort(fs, t) < 0) {
            //call transaction recovery
        }
        free(index_block);
        free(t);
        return -1;
    }

    if (filesystem_flush_block(fs, index_block, inode_copy.index) < 0) {
        if (transaction_abort(fs, t) < 0) {
            //call transaction recovery
        }
        free(index_block);
        free(t);
        return -1;
    }

    if (transaction_commit(fs, t) < 0) {
        if (transaction_abort(fs, t) < 0) {
            //call transaction recovery
        }
        free(index_block);
        free(t);
        return -1;
    }
    free(t);


    if (bitmap_free(fs->bm, index_block[inode_copy.blocks -1].block_number) < 0) {
        //call file recovery
        free(index_block);
        return -1;
    }
    if (bitmap_set_reserved(fs->bm, index_block[inode_copy.blocks -1].block_number) < 0) {
        //BIG NONO THIS IS A CORRUPTION, can currently never happen but if threads become a thing this should be looked at, maybe undo the rework for freeing where it doesnt release reservations
        //call file recovery

        //has no reason to do anything if the previous thing failed
        bitmap_set(fs->bm, index_block[inode_copy.blocks -1].block_number);
        free(index_block);
        return -1;
    }

    int res = bitmap_flush(fs);
    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        //call file recovery
        bitmap_set(fs->bm, index_block[inode_copy.blocks -1].block_number);
        free(index_block);
        return -1;
    }

    if (res == BITMAP_INDETERMINATE) {
        //call bitmap and file recovery
        free(index_block);
        return -1;
    }

    inode_copy.blocks--;
    if (inode_table_set(fs->it, &inode_copy, file->inode) < 0) {
        //call file recovery
        free(index_block);
        return -1;
    }

    if (inode_table_flush(fs) < 0) {
        //call file recovery
        free(index_block);
        return -1;
    }

    if (bitmap_release_reserve(fs->bm, index_block[inode_copy.blocks].block_number) < 0) {
        //SHOULD ABSOLUTELY NEVER HAPPEN
        //call file recovery
        free(index_block);
        return -1;
    }

    res = bitmap_flush(fs);
    if (res == BITMAP_INDETERMINATE) {
        res = bitmap_validate_flush(fs);
    }

    if (res == BITMAP_FAIL) {
        //call file recovery
        free(index_block);
        return -1;
    }

    if (res == BITMAP_INDETERMINATE) {
        //call bitmap and file recovery
        free(index_block);
        return -1;
    }


    index_block[inode_copy.blocks].status = FILE_BLOCK_FREE;

    t = transaction_create(fs);
    if (t == NULL) {
        //call bitmap and file recovery
        free(index_block);
        return -1;
    }

    if (transaction_add_blocks(fs, t, 1, &inode_copy.index) < 0) {
        //call file recovery
        if (transaction_abort(fs, t) < 0) {
            //call transaction recovery
        }
        free(t);
        free(index_block);
        return -1;
    }



    if (filesystem_flush_block(fs, index_block, inode_copy.index) < 0) {
        //call file recovery
        if (transaction_abort(fs, t) < 0) {
            //call transaction recovery
        }
        free(t);
        free(index_block);
        return -1;
    }


    if (transaction_commit(fs, t) < 0) {
        //call file recovery
        if (transaction_abort(fs, t) < 0) {
            //call transaction
        }
        free(t);
        free(index_block);
        return -1;
    }



    free(t);
    free(index_block);

    return 0;

}
