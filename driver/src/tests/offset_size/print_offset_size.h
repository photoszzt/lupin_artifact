#ifndef PRINT_OFFSET_SIZE_H
#define PRINT_OFFSET_SIZE_H 1

#include "log.h"

void print_size(void)
{
    PRINTF("struct list_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", 
        sizeof(struct list_entry), _Alignof(struct list_entry));
    PRINTF("    offsetof pe_next: %lu\n", offsetof(struct list_entry, pe_next));
    PRINTF("    offsetof pe_prev: %lu\n", offsetof(struct list_entry, pe_prev));

    PRINTF("struct buddyop_log_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct buddyop_log_entry), 
        _Alignof(struct buddyop_log_entry));
    PRINTF("    offsetof pos: %lu\n", offsetof(struct buddyop_log_entry, pos));

    PRINTF("struct val_log_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct val_log_entry), 
        _Alignof(struct val_log_entry));
    PRINTF("    offsetof off: %lu\n", offsetof(struct val_log_entry, off));
    PRINTF("    offsetof value: %lu\n", offsetof(struct val_log_entry, value));

    PRINTF("struct str_log_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct str_log_entry), 
        _Alignof(struct str_log_entry));
    PRINTF("    offsetof off: %lu\n", offsetof(struct str_log_entry, off));
    PRINTF("    offsetof val: %lu\n", offsetof(struct str_log_entry, val));

    PRINTF("struct commit_log_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct commit_log_entry), 
        _Alignof(struct commit_log_entry));
    PRINTF("    offsetof check_sum: %lu\n", offsetof(struct commit_log_entry, check_sum));

    PRINTF("log_entry: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct log_entry), _Alignof(struct log_entry));
    PRINTF("    offsetof tp: %lu\n", offsetof(struct log_entry, tp));
    PRINTF("    offsetof entry: %lu\n", offsetof(struct log_entry, entry));

    PRINTF("meta_shmem: \n");
    PRINTF("    size: %lu, align: %lu\n", sizeof(struct meta_shmem), _Alignof(struct meta_shmem));
    PRINTF("    offsetof redolog: %lu\n", offsetof(struct meta_shmem, redolog));
    PRINTF("    offsetof obj_list: %lu\n", offsetof(struct meta_shmem, obj_headers)); 
    PRINTF("    offsetof obj_list: %lu\n", offsetof(struct meta_shmem, obj_allocs)); 
    PRINTF("    offsetof meta_lock: %lu\n", offsetof(struct meta_shmem, meta_lock));
    PRINTF("    offsetof buddy_alloc: %lu\n", offsetof(struct meta_shmem, buddy_alloc));
}

#endif // PRINT_OFFSET_SIZE_H
