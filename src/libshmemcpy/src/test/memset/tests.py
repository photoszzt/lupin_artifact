#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation
#
import argparse
import os
import sys
current = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(current, '..', 'unittest'))

from collections import namedtuple
from run_command import run_local_command

TC = namedtuple('TC', ['offset', 'length'])


class Pmem2Memset():
    filesize = 4 * 1024 * 1024
    envs0 = ()
    envs1 = ()
    wc_workaround = ()
    test_cases = (
        TC(offset=0, length=8),
        TC(offset=13, length=4096)
    )

    def run(self, binary_path: str):
        pass_env = {"ASAN_OPTIONS": "strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:detect_invalid_pointer_pairs=2"}
        for env in self.envs0:
            pass_env[env] = '0'
        for env in self.envs1:
            pass_env[env] = '1'

        file_path = '/mnt/cxl_mem/mem_1'
        for work_around in self.wc_workaround:
            if work_around == 'on':
                pass_env['SHMEM_WC_WORKAROUND'] = '1'
            elif work_around == 'off':
                pass_env['SHMEM_WC_WORKAROUND'] = '0'

            for tc in self.test_cases:
                run_local_command(['truncate', '-s', '4M', file_path])
                run_local_command([binary_path, file_path, tc.offset, tc.length],
                                  env=pass_env)


class TEST0(Pmem2Memset):
    wc_workaround = ('on', 'off', 'default')


class TEST1(Pmem2Memset):
    envs1 = ("SHMEM_NO_MOVNT",)
    wc_workaround = ('default',)


class TEST2(Pmem2Memset):
    envs1 = ("SHMEM_NO_MOVNT", "SHMEM_NO_GENERIC_MEMCPY")
    wc_workaround = ('default',)


class TEST3(Pmem2Memset):
    envs0 = ("SHMEM_AVX512F",)
    wc_workaround = ('on', 'off', 'default')


class TEST4(Pmem2Memset):
    envs0 = ("SHMEM_AVX512F", "SHMEM_AVX",)
    wc_workaround = ('on', 'off', 'default')


class TEST5(Pmem2Memset):
    envs1 = ("SHMEM_MOVDIR64B",)
    wc_workaround = ('on', 'off', 'default')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True)
    args = parser.parse_args()
    cxl_dir = '/mnt/cxl_mem'
    os.makedirs(cxl_dir, exist_ok=True)
    if not os.path.ismount(cxl_dir):
        run_local_command(['sudo', 'mount', '-t', 'tmpfs', '-o', 'size=4G',
            '-o', 'mpol=bind:1', '-o', 'rw,nosuid,nodev', 'tmpfs', cxl_dir])

    t = TEST0()
    t.run(args.binary)
    t = TEST1()
    t.run(args.binary)
    t = TEST2()
    t.run(args.binary)
    t = TEST3()
    t.run(args.binary)
    t = TEST4()
    t.run(args.binary)
    t = TEST5()
    t.run(args.binary)


if __name__ == '__main__':
    main()