#ifndef _INTERNAL_RETURNS_H_
#define _INTERNAL_RETURNS_H_

#include <misc/cpp_attributes.h>
#include <stdint.h>

static constexpr const uint32_t WAS_PREEMPTED = ~0;

static constexpr const uint64_t FAILED_VEC_FULL = 0x1;
static constexpr const uint64_t FAILED_RSEQ     = 0;

constexpr uint32_t ALWAYS_INLINE CONST_ATTR
successful(const uint64_t ret_val) {
    return ret_val > FAILED_VEC_FULL;
}

constexpr uint32_t ALWAYS_INLINE CONST_ATTR
failed_full(const uint64_t ret_val) {
    return ret_val;
}

constexpr uint32_t ALWAYS_INLINE CONST_ATTR
failed_rseq(const uint64_t ret_val) {
    return ret_val == FAILED_RSEQ;
}


#endif
