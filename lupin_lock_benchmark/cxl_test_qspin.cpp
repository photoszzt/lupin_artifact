#include "linux/queued_spinlock.h"
#include <stdio.h>

#include "atomics.h"
#include "cxl_setup.hpp"
#include "cxlalloc.h"
#include "thread_helper.hpp"

#define THREADS_NUM 32
#define LOCK_NUM 50000

static uint64_t global_lock;
static pthread_barrier_t barrier;
static std::atomic<uint64_t> *global_count = nullptr;
static bool count = false;

static void *measure_lpttas(void *arg) {
  struct timespec start, end;
  std::atomic<uint64_t> lock_acquired;
  uint64_t thread_id;
  thread_id = (uint64_t)arg;
  lock_acquired = 0;

  cxlalloc_init(CXL_NAME, default_cxl_mem_size, thread_id, THREADS_NUM + 1, 0,
                1);

  pin_thread(thread_id);
  pthread_barrier_wait(&barrier);
  // printf("thread_id = %ld, node addr = %p\n", thread_id, &node);

  if (count) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    while (lock_acquired < LOCK_NUM) {
      lock_acquire(&global_lock, thread_id);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      global_count->fetch_add(1, std::memory_order_relaxed);
      lock_release(&global_lock, thread_id);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  } else {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    while (lock_acquired < LOCK_NUM) {
      lock_acquire(&global_lock, thread_id);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      lock_release(&global_lock, thread_id);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  }
  uint64_t start_ns = start.tv_sec * 1000000000ul + start.tv_nsec;
  uint64_t end_ns = end.tv_sec * 1000000000ul + end.tv_nsec;
  printf("[%lu] qspin_cxl mean passage latency (us): %lf\n", thread_id,
         (double)(end_ns - start_ns) / (double)LOCK_NUM / 1000.0);
  return nullptr;
}

int main(void) {
  pthread_t p_thds[THREADS_NUM];
  struct sched_param sparam;
  pthread_attr_t attr;

  count = run_global_count();
  /* init cxlalloc */
  cxlalloc_init(CXL_NAME, default_cxl_mem_size, THREADS_NUM, THREADS_NUM + 1, 0,
                1);
  printf("cxlalloc init succeed!, size = %ld bytes\n", default_cxl_mem_size);

  mcs_init_locks(&global_lock, THREADS_NUM, true);
  global_count = (std::atomic<uint64_t> *)cxlalloc_memalign(
      sizeof(std::atomic<uint64_t>), CACHELINE_SIZE);
  global_count->store(0);

  pthread_attr_init(&attr);
  set_pthread_attr(&sparam, &attr);
  pthread_barrier_init(&barrier, nullptr, THREADS_NUM + 1);

  printf("test start\n");
  for (int i = 0; i < THREADS_NUM; i++) {
    uint64_t vid = i;
    pthread_create(&p_thds[i], &attr, measure_lpttas, (void *)vid);
  }
  pthread_barrier_wait(&barrier);

  for (int i = 0; i < THREADS_NUM; i++) {
    pthread_join(p_thds[i], nullptr);
  }
  pthread_barrier_destroy(&barrier);
  pthread_attr_destroy(&attr);
  cxlalloc_free(global_count);

  printf("test finished\n");
  return 0;
}
