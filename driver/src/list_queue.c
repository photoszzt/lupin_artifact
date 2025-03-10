#include "log.h"

// use two log entry
void record_list_queue_append(struct log_entry redolog[], int *redolog_pos_ptr,
			      struct list_queue *lq, struct list_entry *le,
			      uint8_t *mem_start)
{
	struct shmem_optr pe_first_optr;
	struct shmem_optr tail_optr;
	struct shmem_optr prev_optr;
	struct shmem_optr next_optr;
	struct shmem_optr cur_tail_next_optr;

	struct shmem_optr le_optr =
		shmem_optr_from_ptr((uintptr_t)mem_start, le);
	prev_optr = le_prev_optr(le_optr);
	next_optr = le_next_optr(le_optr);

	if (shmem_optr_is_null(lq->pe_first)) {
		// first obj prev optr points to head which is pe_first
		pe_first_optr = shmem_optr_from_ptr((uintptr_t)mem_start,
						    &lq->pe_first);
		// set_value_log_entry(redolog, redolog_pos_ptr, prev_optr, 0);
		// pe_first optr points to list_entry
		// set_value_log_entry(redolog, redolog_pos_ptr, pe_first_optr, le_off);
		set_two_value_log_entry(redolog, redolog_pos_ptr, prev_optr, 0,
					pe_first_optr, le_optr.off);
	} else {
		cur_tail_next_optr = le_next_optr(lq->pe_tail);
		// le->prev = current tail
		// le->prev->next = le
		set_two_value_log_entry(redolog, redolog_pos_ptr, prev_optr,
					lq->pe_tail.off, cur_tail_next_optr,
					le_optr.off);
	}

	tail_optr = shmem_optr_from_ptr((uintptr_t)mem_start, &lq->pe_tail);
	/* list_entry's next optr points to NULL */
	// set_value_log_entry(redolog, redolog_pos_ptr, next_optr, 0);

	/* update current tail to list_entry */
	// set_value_log_entry(redolog, redolog_pos_ptr, tail_optr, le_off);
	set_two_value_log_entry(redolog, redolog_pos_ptr, next_optr, 0,
				tail_optr, le_optr.off);
}

// use 1 or 2 log entries
void record_list_queue_remove(struct log_entry redolog[], int *redolog_pos_ptr,
			      struct list_queue *lq, struct list_entry *le,
			      uint8_t *mem_start)
{
	struct shmem_optr le_optr;
	struct shmem_optr pe_first_optr;
	BUG_ON(mem_start == NULL);
	BUG_ON(lq == NULL);
	BUG_ON(redolog_pos_ptr == NULL);
	BUG_ON(le == NULL);
	le_optr = shmem_optr_from_ptr((uintptr_t)mem_start, le);

	/* Only one element on list */
	if (shmem_optr_equals(lq->pe_first, le_optr) &&
	    shmem_optr_is_null(le->pe_next)) {
		/* set pe_first to NULL */
		pe_first_optr = shmem_optr_from_ptr((uintptr_t)mem_start,
						    &lq->pe_first);
		set_value_log_entry(redolog, redolog_pos_ptr, pe_first_optr, 0);
	} else {
		/* set next->prev == prev and prev->next == next */
		struct shmem_optr next_prev_optr;
		struct shmem_optr prev_next_optr;
		next_prev_optr = le_prev_optr(le->pe_next);
		prev_next_optr = le_next_optr(le->pe_prev);
		// set_value_log_entry(redolog, redolog_pos_ptr,
		//     next_prev_optr, le->pe_prev.off);
		// set_value_log_entry(redolog, redolog_pos_ptr,
		//     prev_next_optr, le->pe_next.off);
		set_two_value_log_entry(redolog, redolog_pos_ptr,
					next_prev_optr, le->pe_prev.off,
					prev_next_optr, le->pe_next.off);
		/* if deleting the first entry */
		if (shmem_optr_equals(lq->pe_first, le_optr)) {
			/* set pe_first == next */
			pe_first_optr = shmem_optr_from_ptr(
				(uintptr_t)mem_start, &lq->pe_first);
			set_value_log_entry(redolog, redolog_pos_ptr,
					    pe_first_optr, le->pe_next.off);
		}
	}
}
