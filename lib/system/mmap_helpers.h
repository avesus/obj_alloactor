#ifndef _MMAP_HELPERS_H_
#define _MMAP_HELPERS_H_

#include <sys/mman.h>

#include <misc/error_handling.h>
#include <system/sys_info.h>

namespace MMAP {

#define mmap_alloc_reserve(length)                                             \
    safe_mmap(NULL length,                                                     \
              (PROT_READ | PROT_WRITE),                                        \
              (MAP_ANONYMOUS | MAP_PRIVATE),                                   \
              (-1),                                                            \
              0)

#define mmap_alloc_noreserve(length)                                           \
    safe_mmap(NULL,                                                            \
              length,                                                          \
              (PROT_READ | PROT_WRITE),                                        \
              (MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE),                   \
              (-1),                                                            \
              0)

#define mmap_alloc_pagein_reserve(length, npages)                              \
    MMAP::mmap_pagein(                                                         \
        (uint8_t * const)safe_mmap(NULL,                                       \
                                   length,                                     \
                                   (PROT_READ | PROT_WRITE),                   \
                                   (MAP_ANONYMOUS | MAP_PRIVATE),             \
                                   (-1),                                       \
                                   0),                                         \
        npages)

#define mmap_alloc_pagein_noreserve(length, npages)                            \
    MMAP::mmap_pagein((uint8_t * const)safe_mmap(                              \
                          NULL,                                                \
                          length,                                              \
                          (PROT_READ | PROT_WRITE),                            \
                          (MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE),       \
                          (-1),                                                \
                          0),                                                  \
                      npages)


#define safe_mmap(addr, length, prot_flags, mmap_flags, fd, offset)            \
    MMAP::_safe_mmap((addr),                                                   \
                     (length),                                                 \
                     (prot_flags),                                             \
                     (mmap_flags),                                             \
                     (fd),                                                     \
                     (offset),                                                 \
                     __FILE__,                                                 \
                     __LINE__)

#define safe_munmap(addr, length)                                              \
    MMAP::_safe_munmap(addr, length, __FILE__, __LINE__)


void *
_safe_mmap(void *        addr,
           uint64_t      length,
           int32_t       prot_flags,
           int32_t       mmap_flags,
           int32_t       fd,
           int32_t       offset,
           const char *  fname,
           const int32_t ln) {

    void * p = mmap(addr, length, prot_flags, mmap_flags, fd, offset);
    ERROR_ASSERT(p != MAP_FAILED,
                 "%s:%d mmap(%p, %lu...) failed\n",
                 fname,
                 ln,
                 addr,
                 length);
    return p;
}

void *
mmap_pagein(uint8_t * const p, uint32_t npages) {

    // this will cause memory to page now to avoid overhead later
    for (uint32_t i = 0; i < PAGE_SIZE * npages; i += PAGE_SIZE) {
        p[i] = 0;
    }
    return (void *)p;
}


void
_safe_munmap(void *        addr,
             uint64_t      length,
             const char *  fname,
             const int32_t ln) {
    ERROR_ASSERT(munmap(addr, length) != -1,
                 "%s:%d munmap(%p, %lu) failed\n",
                 fname,
                 ln,
                 addr,
                 length);
}

}  // namespace MMAP

#endif
