#pragma once
#ifndef _COMMON_CPP_CXLALLOC_HPP_
#define _COMMON_CPP_CXLALLOC_HPP_
// #include <memory_resource>
// #include <type_traits>
// #include <utility>

#include <immintrin.h>

#include <boost/interprocess/offset_ptr.hpp>
#include <memory>

#include "../common/common_macro.h"
#include "../cxlalloc/include/cxlalloc.h"
namespace bip = boost::interprocess;

/**
 * Suppose we have a ptr s that points to an object obj
 * cxlalloc_unlink is not cxlalloc_free. cxlalloc_free takes the ptr to the
 * object to free(obj in the above
 * example). cxlalloc_unlink takes the address of the offset_ptr's m_offset
 * field
 */

/*
template <typename T>
struct CXLAlloc {
  using value_type = T;
  using void_pointer = bip::offset_ptr<void>;
  using const_void_pointer = bip::offset_ptr<const void>;
  using pointer =
      typename std::pointer_traits<void_pointer>::template rebind<T>;
  using const_pointer =
      typename std::pointer_traits<void_pointer>::template rebind<const T>;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

  [[nodiscard]] pointer allocate(
      size_type count,
      const_void_pointer hint = bip::offset_ptr<const void>{nullptr}) {
    (void)hint;
    return pointer(
        static_cast<value_type*>(cxlalloc_malloc(count * sizeof(value_type))));
  }

  void deallocate(pointer p, [[maybe_unused]] size_type n) {
    cxlalloc_unlink(std::addressof(p.priv_offset()));
  }
};

template <typename T, typename U>
inline bool operator==(const CXLAlloc<T>&, const CXLAlloc<U>&) noexcept {
  return true;
}

template <typename T, typename U>
inline bool operator!=(const CXLAlloc<T>&, const CXLAlloc<U>&) noexcept {
  return false;
}
*/

// WARN: the constructor cannot have inner allocations
template <typename T, class... Args>
T* cxl_construct_nptr(Args&&... args) {
  T* addr = static_cast<T*>(cxlalloc_malloc(sizeof(T)));
  if (unlikely(addr == nullptr)) {
    return nullptr;
  }
  std::construct_at(addr, args...);
  return addr;
}

template <typename T>
void cxl_destroy(T* vptr) {
  if (likely(vptr != nullptr)) {
    std::destroy_at(vptr);
    cxlalloc_free(vptr);
  }
}

template <typename T>
force_inline void delete_alloc(T* ptr) {
  if (likely(ptr != nullptr)) {
#ifdef USE_CXLALLOC
    std::destroy_at(ptr);
    cxlalloc_free(ptr);
#else
    delete ptr;
#endif
  }
}

template <typename T, bool useCxlAlloc>
force_inline void delete_alloc_temp(T* ptr) {
  if (likely(ptr != nullptr)) {
    if constexpr (useCxlAlloc) {
      std::destroy_at(ptr);
      cxlalloc_free(ptr);
    } else {
      delete ptr;
    }
  }
}

// WARN: the constructor cannot have inner allocations
template <typename T, class... Args>
T* mem_construct_nptr(Args&&... args) {
#ifdef USE_CXLALLOC
  T* addr = static_cast<T*>(cxlalloc_malloc(sizeof(T)));
  if (unlikely(addr == nullptr)) {
    fprintf(stderr, "[%s](%d) fail to allocate CXL memory\n", __FILE__,
            __LINE__);
    return nullptr;
  }
  std::construct_at(addr, args...);
  return addr;
#else
  return new T(args...);
#endif
}

template <typename T, bool useCxlAlloc, class... Args>
T* mem_construct_nptr_temp(Args&&... args) {
  if constexpr (useCxlAlloc) {
    T* addr = static_cast<T*>(cxlalloc_malloc(sizeof(T)));
    if (unlikely(addr == nullptr)) {
      fprintf(stderr, "[%s](%d) fail to allocate CXL memory\n", __FILE__,
              __LINE__);
      return nullptr;
    }
    std::construct_at(addr, args...);
    return addr;
  } else {
    return new T(args...);
  }
}

#ifdef USE_OFFSETPTR
template <typename T>
force_inline bip::offset_ptr<T>* cxlalloc_ptr_array(size_t arr_len) {
  return static_cast<bip::offset_ptr<T>*>(
      cxlalloc_malloc(sizeof(bip::offset_ptr<T>) * arr_len));
}
#else
template <typename T>
force_inline T** cxlalloc_ptr_array(size_t arr_len) {
  return static_cast<T**>(cxlalloc_malloc(sizeof(T*) * arr_len));
}
#endif

#ifdef USE_OFFSETPTR
template <typename T>
force_inline bip::offset_ptr<T>* alloc_ptr_array(size_t arr_len) {
#ifdef USE_CXLALLOC
  return static_cast<bip::offset_ptr<T>*>(
      cxlalloc_malloc(sizeof(bip::offset_ptr<T>) * arr_len));
#else
  return new bip::offset_ptr<T>[arr_len];
#endif
}
#else
template <typename T>
force_inline T** alloc_ptr_array(size_t arr_len) {
#ifdef USE_CXLALLOC
  return static_cast<T**>(cxlalloc_malloc(sizeof(T*) * arr_len));
#else
  return new T*[arr_len];
#endif
}
#endif

force_inline void free_alloc(void* ptr) {
  if (likely(ptr != nullptr)) {
#ifdef USE_CXLALLOC
    cxlalloc_free(ptr);
#else
    free(ptr);
#endif
  }
}

force_inline void* mem_alloc(size_t s) {
#ifdef USE_CXLALLOC
  return cxlalloc_malloc(s);
#else
  return malloc(s);
#endif
}

template <bool useCxlAlloc>
force_inline void free_alloc_temp(void* ptr) {
  if constexpr (useCxlAlloc) {
    cxlalloc_free(ptr);
  } else {
    free(ptr);
  }
}

template <bool useCxlAlloc>
force_inline void* mem_alloc_temp(size_t s) {
  if constexpr (useCxlAlloc) {
    return cxlalloc_malloc(s);
  } else {
    return malloc(s);
  }
}

template <bool useCxlAlloc>
WARN_UNUSED_RESULT force_inline void* mem_aligned_alloc_temp(size_t alignment,
                                                             size_t s) {
  if constexpr (useCxlAlloc) {
    return cxlalloc_memalign(alignment, s);
  } else {
    void* ret;
    int err = posix_memalign(&ret, alignment, s);
    if (likely(err == 0)) {
      return ret;
    }
    if (err == EINVAL) {
      fprintf(stderr, "posix_memalign invalid arg\n");
    } else if (err == ENOMEM) {
      fprintf(stderr, "posix_memalign out of memory\n");
    }
    return ret;
  }
}

template <typename T>
WARN_UNUSED_RESULT T* wait_for_init(size_t idx, T* ptr) {
  while (ptr == nullptr) {
    ptr = static_cast<T*>(cxlalloc_get_root(idx, nullptr));
    _mm_pause();
  }
  return ptr;
}

template <typename T>
void print_root_offset(T* ptr, const char* s) {
  uint64_t off = 0;
  auto ret = cxlalloc_pointer_to_offset(ptr, &off);
  if (ret) {
    fprintf(stderr, "%s %lu\n", s, off);
  } else {
    fprintf(stderr, "fail to get %s\n", s);
  }
}

#endif  // _COMMON_CPP_CXLALLOC_HPP_
