#ifndef COMMON_CPP_CRASH_HERE_H
#define COMMON_CPP_CRASH_HERE_H

// from libbsd: https://cgit.freedesktop.org/libbsd/tree/include/bsd/sys/time.h
#ifndef timespecsub
#define timespecsub(tsp, usp, vsp)                    \
  do {                                                \
    (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;    \
    (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec; \
    if ((vsp)->tv_nsec < 0) {                         \
      (vsp)->tv_sec--;                                \
      (vsp)->tv_nsec += 1000000000L;                  \
    }                                                 \
  } while (0)
#endif
#ifdef CRASH
void crash_here_rand(int crash_num, volatile struct timespec* time);
void crash_here_rand(int crash_num);
void crash_here_rand();
#else
#define crash_here_rand(...)
#endif
void init_crash_module();

#endif  // COMMON_CPP_CRASH_HERE_H
