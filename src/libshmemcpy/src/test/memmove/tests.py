#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
import argparse
import os
import sys
current = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(current, '..', 'unittest'))

from run_command import run_local_command

class Pmem2Memmove():
    filesize = 4 * 1024 * 1024
    envs0 = ()
    envs1 = ()
    test_cases = [
        # No offset, no overlap
        ['b:4096'],

        # aligned dest, unaligned source, no overlap
        ['s:7', 'b:4096'],

        # unaligned dest, unaligned source, no overlap
        ['d:7', 's:13', 'b:4096'],

        # all aligned, src overlaps dest
        ['b:4096', 's:23', 'o:1'],

        # unaligned destination
        ['b:4096', 'd:21'],

        # unaligned source and dest
        ['b:4096', 'd:21', 's:7'],

        # overlap of src, aligned src and dest
        ['b:4096', 'o:1', 's:20'],

        # overlap of src, aligned src, unaligned dest
        ['b:4096', 'd:13', 'o:1', 's:20'],

        # dest overlaps src, unaligned dest, aligned src
        ['b:2048', 'd:33', 'o:1'],

        # dest overlaps src, aligned dest and src
        ['b:4096', 'o:1', 'd:20'],

        # aligned dest, no overlap, small length
        ['b:8'],

        # small length, offset 1 byte from 64 byte boundary
        ['b:4', 'd:63'],

        # overlap, src < dest, small length (ensures a copy backwards,
        # with number of bytes to align < length)
        ['o:1', 'd:2', 'b:8']
    ]

    def run(self, binary_path: str):
        pass_env = {"ASAN_OPTIONS": "strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:detect_invalid_pointer_pairs=2"}
        for env in self.envs0:
            pass_env[env] = '0'
        for env in self.envs1:
            pass_env[env] = '1'
        file_path = '/mnt/cxl_mem/mem_1'
        for tc in self.test_cases:
            run_local_command(['truncate', '-s', '4M', file_path])
            run_local_command([binary_path, file_path, *tc], env=pass_env)


class TEST0(Pmem2Memmove):
    pass


class TEST1(Pmem2Memmove):
    envs0 = ("SHMEM_AVX512F",)


class TEST2(Pmem2Memmove):
    envs0 = ("SHMEM_AVX512F", "SHMEM_AVX",)


class TEST3(Pmem2Memmove):
    envs1 = ("SHMEM_NO_MOVNT",)


class TEST4(Pmem2Memmove):
    envs1 = ("SHMEM_NO_MOVNT", "SHMEM_NO_GENERIC_MEMCPY")


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