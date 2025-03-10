#ifndef DRAM_TTAS_H_
#define DRAM_TTAS_H_

// LiTL: Library for Transparent Lock interposition
#include "common_macro.h"
#include <stdint.h>

#define r_align(n, r) (((n) + (r) - 1) & -(r))
#define cache_align(n) r_align(n, L_CACHE_LINE_SIZE)
#define pad_to_cache_line(n) (cache_align(n) - (n))
#define L_CACHE_LINE_SIZE 64

static inline uint8_t l_tas_uint8(volatile uint8_t *addr) {
  uint8_t oldval;
  __asm__ __volatile__("xchgb %0,%1"
                       : "=q"(oldval), "=m"(*addr)
                       : "0"((unsigned char)0xff), "m"(*addr)
                       : "memory");
  return (uint8_t)oldval;
}

typedef struct ttas_mutex {
  volatile uint8_t spin_lock __attribute__((aligned(L_CACHE_LINE_SIZE)));
  char __pad[pad_to_cache_line(sizeof(uint8_t))];
} ttas_mutex_t __attribute__((aligned(L_CACHE_LINE_SIZE)));

#define LOCKED 0
#define UNLOCKED 1

void ttas_mutex_lock(ttas_mutex_t *impl) {
  while (1) {
    while (impl->spin_lock != UNLOCKED) {
      CPU_PAUSE();
    }
    if (l_tas_uint8(&impl->spin_lock) == UNLOCKED) {
      break;
    }
  }
}

void ttas_mutex_unlock(ttas_mutex_t *impl) {
  COMPILER_BARRIER();
  impl->spin_lock = UNLOCKED;
}

#endif // DRAM_TTAS_H_
