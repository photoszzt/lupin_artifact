#include <stdio.h>

#include "atomics.h"
#include "thread_helper.hpp"

#define wsize 32
#define JJ_WPORT_SPIN_IMPL
#include "jj_ab_spin/jj_ab_wport_spinlock.h"

static struct jj_wport_spin(wsize) *global_lock = nullptr;
static pthread_barrier_t barrier;
static std::atomic<uint64_t> *global_count = nullptr;
static bool count = false;

#define THREADS_NUM 32
#define LOCK_NUM 50000

static void *measure_jj_wport(void *arg) {
  struct timespec start, end;
  std::atomic<uint64_t> lock_acquired = 0;
  uint64_t thread_id;
  thread_id = (uint64_t)arg;

  // printf("thread_id = %ld\n", thread_id);
  pin_thread(thread_id);
  pthread_barrier_wait(&barrier);

  if (count) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    while (lock_acquired < LOCK_NUM) {
      jj_wport_spin_lock_f(wsize)(global_lock, thread_id);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      global_count->fetch_add(1, std::memory_order_relaxed);
      jj_wport_spin_unlock_f(wsize)(global_lock, thread_id);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  } else {
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    while (lock_acquired < LOCK_NUM) {
      jj_wport_spin_lock_f(wsize)(global_lock, thread_id);
      lock_acquired.fetch_add(1, std::memory_order_relaxed);
      jj_wport_spin_unlock_f(wsize)(global_lock, thread_id);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  }
  uint64_t start_ns = start.tv_sec * 1000000000ul + start.tv_nsec;
  uint64_t end_ns = end.tv_sec * 1000000000ul + end.tv_nsec;
  printf("[%lu] JJ32port_dram mean passage latency (us): %lf\n", thread_id,
         (double)(end_ns - start_ns) / (double)LOCK_NUM / 1000.0);
  return nullptr;
}

int main(void) {
  pthread_t p_thds[THREADS_NUM];
  struct sched_param sparam;
  pthread_attr_t attr;

  count = run_global_count();
  posix_memalign((void **)&global_lock, CACHELINE_SIZE,
                 sizeof(struct jj_wport_spin(wsize)));
  memset(global_lock, 0, sizeof(struct jj_wport_spin(wsize)));
  jj_wport_spin_init_f(wsize)(global_lock);

  posix_memalign((void **)&global_count, CACHELINE_SIZE,
                 sizeof(std::atomic<uint64_t>));
  global_count->store(0);

  pthread_attr_init(&attr);
  set_pthread_attr(&sparam, &attr);
  pthread_barrier_init(&barrier, nullptr, THREADS_NUM + 1);

  printf("test start\n");
  for (int i = 0; i < THREADS_NUM; i++) {
    uint64_t vid = i;
    pthread_create(&p_thds[i], &attr, measure_jj_wport, (void *)vid);
  }
  pthread_barrier_wait(&barrier);

  for (int i = 0; i < THREADS_NUM; i++) {
    pthread_join(p_thds[i], nullptr);
  }
  pthread_barrier_destroy(&barrier);
  pthread_attr_destroy(&attr);
  free(global_count);
  free(global_lock);

  printf("test finished\n");
  return 0;
}
