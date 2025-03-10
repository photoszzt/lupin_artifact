#ifndef THREAD_HELPER_H
#define THREAD_HELPER_H

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <string.h>

static inline void set_pthread_attr(struct sched_param *sparam,
                                    pthread_attr_t *attr) {
  pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(attr, SCHED_FIFO);
  sparam->sched_priority = 1;
  pthread_attr_setschedparam(attr, sparam);
}

static inline void sync_start(std::atomic<std::uint64_t> *ready,
                              uint64_t target) {
  ready->fetch_add(1, std::memory_order_acq_rel);
  while (ready->load(std::memory_order_acquire) != target)
    ;
}

static inline void pin_thread(uint64_t thread_id) {
  int res;
  cpu_set_t affin_mask;
  CPU_ZERO(&affin_mask);
  CPU_SET(thread_id, &affin_mask);
  res = sched_setaffinity(0, sizeof(cpu_set_t), &affin_mask);
  if (res == -1) {
    fprintf(stderr, "sched_setaffinity failed: %s\n", strerror(errno));
    exit(1);
  }
}

static inline bool run_global_count() {
  const char *count = std::getenv("GCOUNT");
  bool run_count = false;
  if (count != nullptr && count[0] == '1') {
    run_count = true;
  }
  return run_count;
}

#endif // THREAD_HELPER_H
