#include <stdio.h>
#include <string.h>

#include "atomics.h"
#include "cxl_setup.hpp"
#include "cxlalloc.h"
#include "rmcs.hpp"
#include "thread_helper.hpp"

#define THREADS_NUM 32
#define LOCK_NUM 50000

static Lock *global_lock;
static Qnode *nodes;

static pthread_barrier_t barrier;
static std::atomic<uint64_t> *global_count = nullptr;
static bool count = false;

static void *measure_rmcs(void *arg) {
  struct timespec start, end;
  std::atomic<uint64_t> lock_acquired;
  uint64_t thread_id;
  [[maybe_unused]] bool ret;
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
      ret = rmcs_lock(global_lock, &nodes[thread_id]);
      // assert(ret == true);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      global_count->fetch_add(1, std::memory_order_relaxed);
      ret = rmcs_unlock(global_lock, &nodes[thread_id]);
      // assert(ret == true);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  } else {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    while (lock_acquired < LOCK_NUM) {
      ret = rmcs_lock(global_lock, &nodes[thread_id]);
      // assert(ret == true);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      ret = rmcs_unlock(global_lock, &nodes[thread_id]);
      // assert(ret == true);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  }
  uint64_t start_ns = start.tv_sec * 1000000000ul + start.tv_nsec;
  uint64_t end_ns = end.tv_sec * 1000000000ul + end.tv_nsec;
  printf("[%lu] RMCS_cxl mean passage latency (us): %lf\n", thread_id,
         (double)(end_ns - start_ns) / (double)LOCK_NUM / 1000.0);
  return NULL;
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

  global_lock = (Lock *)cxlalloc_memalign(sizeof(Lock), CACHELINE_SIZE);
  nodes =
      (Qnode *)cxlalloc_memalign(sizeof(Qnode) * THREADS_NUM, CACHELINE_SIZE);
  memset(nodes, 0, sizeof(Qnode) * THREADS_NUM);

  global_lock->tail = nullptr;
  global_lock->clean_cnt = 0;
  global_lock->clean_in_prog = false;
  global_count = (std::atomic<uint64_t> *)cxlalloc_memalign(
      sizeof(std::atomic<uint64_t>), CACHELINE_SIZE);
  global_count->store(0);

  pthread_attr_init(&attr);
  set_pthread_attr(&sparam, &attr);
  pthread_barrier_init(&barrier, nullptr, THREADS_NUM + 1);

  printf("test start\n");
  for (int i = 0; i < THREADS_NUM; i++) {
    uint64_t vid = i;
    pthread_create(&p_thds[i], &attr, measure_rmcs, (void *)vid);
  }
  pthread_barrier_wait(&barrier);

  for (int i = 0; i < THREADS_NUM; i++) {
    pthread_join(p_thds[i], nullptr);
  }
  pthread_barrier_destroy(&barrier);
  pthread_attr_destroy(&attr);
  cxlalloc_free(global_count);
  cxlalloc_free(global_lock);

  printf("test finished\n");
  return 0;
}
