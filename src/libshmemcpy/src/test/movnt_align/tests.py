#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation
#


class MovntAlignCommon():
    filesize = 4 * 1024 * 1024

    def run_cases(self, ctx):
        ctx.exec('shmem_movnt_align', self.filepath, "C")
        ctx.exec('shmem_movnt_align', self.filepath, "F")
        ctx.exec('shmem_movnt_align', self.filepath, "B")
        ctx.exec('shmem_movnt_align', self.filepath, "S")

    def run(self, ctx):
        self.filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        self.run_cases(ctx)


class Pmem2MovntAlign(MovntAlignCommon):
    threshold = None
    threshold_values = ['0', '99999']
    envs0 = ()

    def run(self, ctx):
        for env in self.envs0:
            ctx.env[env] = '0'

        super().run(ctx)
        for tv in self.threshold_values:
            ctx.env['SHMEM_MOVNT_THRESHOLD'] = tv
            self.run_cases(ctx)


# @t.require_valgrind_enabled('pmemcheck')
# class MovntAlignCommonValgrind(Pmem2MovntAlign):
#     test_type = t.Medium
# 
#     def run(self, ctx):
#         ctx.env['VALGRIND_OPTS'] = "--mult-stores=yes"
#         super().run(ctx)


class TEST0(Pmem2MovntAlign):
    pass


class TEST1(Pmem2MovntAlign):
    envs0 = ("SHMEM_AVX512F",)


class TEST2(Pmem2MovntAlign):
    envs0 = ("SHMEM_AVX512F", "SHMEM_AVX",)


class TEST3(MovntAlignCommon):
    def run(self, ctx):
        ctx.env['SHMEM_NO_MOVNT'] = '1'
        super().run(ctx)


class TEST4(MovntAlignCommon):
    def run(self, ctx):
        ctx.env['SHMEM_NO_MOVNT'] = '1'
        ctx.env['SHMEM_NO_GENERIC_MEMCPY'] = '1'
        super().run(ctx)


# class TEST5(MovntAlignCommonValgrind):
#     pass
# 
# 
# @t.require_architectures('x86_64')
# class TEST6(MovntAlignCommonValgrind):
#     envs0 = ("SHMEM_AVX512F",)
# 
# 
# @t.require_architectures('x86_64')
# class TEST7(MovntAlignCommonValgrind):
#     envs0 = ("SHMEM_AVX512F", "SHMEM_AVX",)
# 
# 
# class TEST8(MovntAlignCommonValgrind):
#     def run(self, ctx):
#         ctx.env['SHMEM_NO_MOVNT'] = '1'
#         super().run(ctx)
# 
# 
# class TEST9(MovntAlignCommonValgrind):
#     def run(self, ctx):
#         ctx.env['SHMEM_NO_MOVNT'] = '1'
#         ctx.env['SHMEM_NO_GENERIC_MEMCPY'] = '1'
#         super().run(ctx)


class TEST10(Pmem2MovntAlign):
    envs1 = ("SHMEM_MOVDIR64B",)
