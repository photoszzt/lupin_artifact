#ifndef VCXL_DEF_H_
#define VCXL_DEF_H_

/* First chunk is reserved for the futex communication queues, 5 pages */
#define FUTEX_ADDR_QUEUE_SIZE 20480
/* Second chunk is reserved for the initial obj allocations record, 4 pages */
#define OBJ_ALLOC_RECS_SIZE 16384
/* 3rd chunk is reserved for death notification queue */
#define PROC_DEATH_NOTIFICATION_SIZE 20480
/* 4th chunk is reserved for CxlCG */
#define CXLCG_META_SIZE 40960

#define PFX "[CXL_IVPCI] "
#define REDOLOG_ENTRIES 10

#endif // VCXL_DEF_H_
