#define _POSIX_C_SOURCE 200112L

#include "../driver_api/cxl_alloc.h"
#include <array>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

constexpr int allocations = 100;

static void
alloc_loop(size_t size, int fd,
           const std::array<std::string, allocations> &names,
           std::array<struct vcxl_find_alloc, allocations> &allocs) {
  struct timespec start, end;
  int res = 0;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for (size_t i = 0; i < allocations; i++) {
    const char *name = names[i].c_str();
    auto ret = find_cxl_alloc_nomap(fd, name, size, &res);
    // assert(ret.ret != -1);
    allocs[i] = std::move(ret);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  uint64_t start_ns =
      (unsigned long)start.tv_sec * 1000000000ul + (unsigned long)start.tv_nsec;
  uint64_t end_ns =
      (unsigned long)end.tv_sec * 1000000000ul + (unsigned long)end.tv_nsec;
  printf("%lu allocation latency (us): %lf\n", size,
         (double)(end_ns - start_ns) / (double)allocations / 1000.0);
}

static void free_loop(size_t size, int fd,
                      const std::array<std::string, allocations> &names,
                      std::array<struct vcxl_find_alloc, allocations> &allocs) {
  int ret;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for (size_t i = 0; i < allocations; i++) {
    const char *name = names[i].c_str();
    ret = cxl_free_shmem(fd, allocs[i].desc.offset, name, size);
    assert(ret != -1);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  uint64_t start_ns =
      (unsigned long)start.tv_sec * 1000000000ul + (unsigned long)start.tv_nsec;
  uint64_t end_ns =
      (unsigned long)end.tv_sec * 1000000000ul + (unsigned long)end.tv_nsec;
  printf("%lu free latency (us): %lf\n", size,
         (double)(end_ns - start_ns) / (double)allocations / 1000.0);
}

int main() {
  int fd = 0;
  fd = open("/dev/cxl_ivpci0", O_RDWR);

  std::array<std::string, allocations> names;
  std::array<vcxl_find_alloc, allocations> allocs;
  for (size_t i = 0; i < allocations; i++) {
    names[i] = std::move(std::to_string(i));
  }
  size_t size = 1024 * 1024;
  alloc_loop(size, fd, names, allocs);
  free_loop(size, fd, names, allocs);

  size = 4 * 1024 * 1024;
  alloc_loop(size, fd, names, allocs);
  free_loop(size, fd, names, allocs);

  size = 16 * 1024 * 1024;
  alloc_loop(size, fd, names, allocs);
  free_loop(size, fd, names, allocs);

  close(fd);
}
