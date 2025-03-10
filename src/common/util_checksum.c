#include <linux/types.h>
#if defined(__KERNEL__) || defined(MODULE)
#include <asm/byteorder.h>
#include <asm/bug.h>
#else
#include <endian.h>
#include <stdlib.h>
#endif
#include "util_checksum.h"

/*
 * util_checksum_compute -- compute Fletcher64-like checksum
 *
 * csump points to where the checksum lives, so that location
 * is treated as zeros while calculating the checksum. The
 * checksummed data is assumed to be in little endian order.
 */
uint64_t
util_checksum_compute(void *addr, size_t len, uint64_t *csump, size_t skip_off)
{
        uint32_t *p32 = addr;
        uint32_t *p32end = (uint32_t *)((char *)addr + len);
        uint32_t *skip;
        uint32_t lo32 = 0;
        uint32_t hi32 = 0;

#if defined(__KERNEL__) || defined(MODULE)
        BUG_ON(len % 4 != 0);
#else
        if (len % 4 != 0) {
                abort();
        }
#endif

        if (skip_off)
                skip = (uint32_t *)((char *)addr + skip_off);
        else
                skip = (uint32_t *)((char *)addr + len);

        while (p32 < p32end)
                if (p32 == (uint32_t *)csump || p32 >= skip) {
                        /* lo32 += 0; treat first 32-bits as zero */
                        p32++;
                        hi32 += lo32;
                        /* lo32 += 0; treat second 32-bits as zero */
                        p32++;
                        hi32 += lo32;
                } else {
#if defined(__KERNEL__) || defined(MODULE)
                        lo32 += le32_to_cpup((__le32 *) p32);
#else
                        lo32 += le32toh(*p32);
#endif
                        ++p32;
                        hi32 += lo32;
                }

        return (uint64_t)hi32 << 32 | lo32;
}

/*
 * util_checksum -- compute Fletcher64-like checksum
 *
 * csump points to where the checksum lives, so that location
 * is treated as zeros while calculating the checksum.
 * If insert is true, the calculated checksum is inserted into
 * the range at *csump.  Otherwise the calculated checksum is
 * checked against *csump and the result returned (true means
 * the range checksummed correctly).
 */
int
util_checksum(void *addr, size_t len, uint64_t *csump,
                int insert, size_t skip_off)
{
        uint64_t csum = util_checksum_compute(addr, len, csump, skip_off);

        if (insert) {
#if defined(__KERNEL__) || defined(MODULE)
                *csump = (__force uint64_t) cpu_to_le64(csum);
#else
                *csump = htole64(csum);
#endif
                return 1;
        }

#if defined(__KERNEL__) || defined(MODULE)
        return *csump == (__force uint64_t) cpu_to_le64(csum);
#else
        return *csump == htole64(csum);
#endif
}

/*
 * util_checksum_seq -- compute sequential Fletcher64-like checksum
 *
 * Merges checksum from the old buffer with checksum for current buffer.
 */
uint64_t
util_checksum_seq(const void *addr, size_t len, uint64_t csum)
{
        const uint32_t *p32 = addr;
        const uint32_t *p32end = (const uint32_t *)((const char *)addr + len);
        uint32_t lo32 = (uint32_t)csum;
        uint32_t hi32 = (uint32_t)(csum >> 32);

#if defined(__KERNEL__) || defined(MODULE)
        BUG_ON(len % 4 != 0);
#else
        if (len % 4 != 0) {
                abort();
        }
#endif

        while (p32 < p32end) {
#if defined(__KERNEL__) || defined(MODULE)
                lo32 += le32_to_cpup((__le32 *) p32);
#else
                lo32 += le32toh(*p32);
#endif
                ++p32;
                hi32 += lo32;
        }
        return (uint64_t)hi32 << 32 | lo32;
}
