mod shmemcpy;
pub use crate::shmemcpy::*;

pub const SHMEM_F_MEM_NODRAIN: u32 = 1;
pub const SHMEM_F_MEM_NONTEMPORAL: u32 = 2;
pub const SHMEM_F_MEM_TEMPORAL: u32 = 4;
pub const SHMEM_F_MEM_WC: u32 = 8;
pub const SHMEM_F_MEM_WB: u32 = 16;
pub const SHMEM_F_MEM_NOFLUSH: u32 = 32;
pub const SHMEM_F_MEM_VALID_FLAGS: u32 = 63;

impl [T] {
    pub fn shmem_copy_from_slice(&mut self, src: &[T])
    where
        T: Copy,
    {
        // The panic code path was put into a cold function to not bloat the
        // call site.
        #[inline(never)]
        #[cold]
        #[track_caller]
        fn len_mismatch_fail(dst_len: usize, src_len: usize) -> ! {
            panic!(
                "source slice length ({}) does not match destination slice length ({})",
                src_len, dst_len,
            );
        }

        if self.len() != src.len() {
            len_mismatch_fail(self.len(), src.len());
        } 
        // SAFETY: `self` is valid for `self.len()` elements by definition, and `src` was
        // checked to have the same length. The slices cannot overlap because
        // mutable references are exclusive.
        shmem_memmove(self.as_mut_ptr(), src.as_ptr(), self.len());
    }
}
