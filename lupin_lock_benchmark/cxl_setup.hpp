#ifndef CXL_DEF_H
#define CXL_DEF_H
#include <stdint.h>

constexpr uint64_t one_giga_bytes = (1024 * 1024 * 1024) + 64 * 1024;
constexpr uint64_t default_cxl_mem_size = one_giga_bytes * 2;
#define CXL_NAME "JJ"

#endif // CXL_DEF_H
