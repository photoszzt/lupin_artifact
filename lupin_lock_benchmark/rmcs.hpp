#pragma once

#include "common_macro.h"
#include <atomic>
#include <cassert>
#include <unistd.h>

#define PAUSE                                                                  \
  { __asm__("pause;"); }

enum LockStatus { WAITING, OWNED, RELEASED };

struct Lock;

struct Qnode {
  Qnode *next;
  LockStatus locked;
  Lock *wants;
  bool vol;
} __attribute__((aligned(CACHELINE_SIZE)));

struct Lock {
  std::atomic<Qnode *> tail;
  int clean_cnt;
  bool clean_in_prog;
} __attribute__((aligned(CACHELINE_SIZE)));

static inline bool rmcs_lock(Lock *lock, Qnode *mynode) {
  Qnode *pred;

  mynode->next = NULL;
  mynode->vol = true;
  mynode->wants = lock;

  std::atomic_thread_fence(std::memory_order_acq_rel);

  if (lock->clean_in_prog) {
    assert(0);
    mynode->next = NULL;
    mynode->vol = false;
    while (lock->clean_in_prog)
      PAUSE;
    return false;
  }

  mynode->locked = WAITING;
  pred = lock->tail.exchange(mynode, std::memory_order_acq_rel);
  if (pred != NULL) {
    pred->next = mynode;

    std::atomic_thread_fence(std::memory_order_acq_rel);

    mynode->vol = false;
    while (mynode->locked != OWNED)
      PAUSE;
  } else {
    mynode->locked = OWNED;

    std::atomic_thread_fence(std::memory_order_acq_rel);

    mynode->vol = false;
  }

  return true;
}

static inline bool rmcs_unlock(Lock *lock, Qnode *mynode) {
  Qnode *expected = mynode;
  mynode->vol = true;

  std::atomic_thread_fence(std::memory_order_acq_rel);
  if (lock->clean_in_prog) {
    assert(0);
    mynode->vol = false;
    while (lock->clean_in_prog)
      PAUSE;
    return false;
  }

  mynode->locked = RELEASED;
  if (mynode->next == NULL) {
    if (lock->tail.compare_exchange_strong(expected, NULL)) {
      expected->vol = false;
      expected->wants = NULL;
      return true;
    }
    int cc = lock->clean_cnt;

    std::atomic_thread_fence(std::memory_order_acq_rel);

    mynode->vol = false;
    while (mynode->next == NULL && cc == lock->clean_cnt &&
           lock->clean_in_prog == false)
      PAUSE;
    mynode->vol = true;

    std::atomic_thread_fence(std::memory_order_acq_rel);

    if (lock->clean_in_prog || cc != lock->clean_cnt) {
      assert(0);
      mynode->vol = false;
      mynode->wants = NULL;
      while (lock->clean_in_prog)
        PAUSE;
      return true;
    }
  }

  std::atomic_thread_fence(std::memory_order_acq_rel);

  mynode->next->locked = OWNED;

  std::atomic_thread_fence(std::memory_order_acq_rel);

  mynode->vol = false;
  mynode->wants = NULL;

  return true;
}

// void cleanup(Lock *lock) {
//   // not implemented
//   assert(0);
// }
