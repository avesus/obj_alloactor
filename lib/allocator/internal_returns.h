#ifndef _INTERNAL_RETURNS_H_
#define _INTERNAL_RETURNS_H_

#include <misc/cpp_attributes.h>

static constexpr const uint64_t FULL_ALLOC_VEC = (~(0UL));
static constexpr const uint64_t EMPTY_FREE_VEC = 0;

static constexpr const uint32_t FAILED_VEC_FULL = 0x1;
static constexpr const uint32_t FAILED_RSEQ     = 0;

constexpr uint32_t ALWAYS_INLINE CONST_ATTR
successful(const uint64_t ret_val) {
    return ret_val > FAILED_VEC_FULL;
}


#endif
