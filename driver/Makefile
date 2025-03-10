CONFIG_MODULE_SIG=n

ccflags-y += -I./src/ -I$(src)/src -I$(src)/../src/common -I$(src)/../src/common/min_array -I$(src)/../src/libshmemcpy/src/include \
    -I$(src)/../src/buddy/include -I$(src)/../src/common/jj_ab_spin -I$(src)/../src/common/km_ab_spin -DVALGRIND_ENABLED=0 $(CFLAGS) \
    -DVG_MEMCHECK_ENABLED=0 -DVG_DRD_ENABLED=0 -DVG_HELGRIND_ENABLED=0 -DVG_PMEMCHECK_ENABLED=0 \
    -std=gnu11 -DBYTE_GRANULAR # allowing defining vars inside a for statement


# src/km_ab_spin/km_64port_uint32spin.o \
# src/jj_ab_spin/jj_ab_spinlock_16proc.o \
# src/jj_ab_spin/jj_ab_spinlock_64proc.o \
# src/jj_ab_spin/jj_ab_64port_spinlock.o \
# src/jjj_spin/jjj_16_spinlock.o
obj-m += cxl_ivpci.o
cxl_ivpci-objs := src/buddy_alloc.o \
                  src/cxl_alloc.o \
                  src/circular_queue.o \
                  src/plist.o src/cxl_ivpci.o \
                  src/cxl_futex_waitwake.o \
                  src/util_checksum.o \
                  src/shmem_ops.o \
                  src/log.o \
                  src/hash_index.o \
                  src/bitset.o \
                  src/obj_header.o \
                  src/list_queue.o \
                  src/ftrace_helper.o \
                  src/random_bytes.o \
                  src/mpsc_queue.o \
                  src/proc_death.o \
                  src/proc_death_netlink.o \
                  src/cxl_cgroup.o

KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

default:
	make -C $(KDIR) M=$(PWD) -j$(nproc) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
