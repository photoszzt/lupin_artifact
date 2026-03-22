#ifndef COMMON_RANDOM_BYTES_H_
#define COMMON_RANDOM_BYTES_H_

#if defined(__KERNEL__) || defined(MODULE)
#include <linux/random.h>
#else
#include <assert.h>
#include <sys/random.h>
#endif
#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

int wait_and_get_random_bytes(void *buf, size_t nbytes) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif // COMMON_RANDOM_BYTES_H_
