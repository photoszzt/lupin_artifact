#ifndef UTIL_CHECKSUM_H_
#define UTIL_CHECKSUM_H_

#include <linux/types.h>
#include "common_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t util_checksum_compute(void *addr, size_t len, uint64_t *csump,
		size_t skip_off) WARN_UNUSED_RESULT;
int util_checksum(void *addr, size_t len, uint64_t *csump,
		int insert, size_t skip_off) WARN_UNUSED_RESULT;
uint64_t util_checksum_seq(const void *addr, size_t len, uint64_t csum) WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif // UTIL_CHECKSUM_H_
