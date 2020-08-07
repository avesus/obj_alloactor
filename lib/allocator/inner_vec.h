#ifndef _INNER_VEC_H_
#define _INNER_VEC_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <optimized/const_math.h>
#include <system/sys_info.h>

#include "rseq/rseq_base.h"

#include <allocator/internal_vec_returns.h>
#include <allocator/safe_atomics.h>

#include <stdint.h>

//////////////////////////////////////////////////////////////////////
// Not part of the api
template<typename wrapped_vecT, uint32_t nvec>
struct inner_allocation_vec {

    uint64_t     v[nvec];
    wrapper_vecT next_vecs[64 * nvec];

    inner_allocation_vec() = default;

    // free_vecs and obj_start will both be computable at compile time. Since
    // this function will be inlined they should be compiled out.
    uint32_t ALWAYS_INLINE
    _allocate(const uint32_t start_cpu, uint64_t * const free_vecs) {
        for (uint32_t i; i < nvec; ++i) {
            if (BRANCH_LIKELY(v[i] != FULL_ALLOC_VEC)) {
                const uint32_t idx = bits::find_first_zero<uint64_t>(v[i]);
                const uint32_t ret =
                    (next_vecs + 64 * i + idx)
                        ->_allocate(start_cpu, free_vecs + 8 * (64 * i + idx));
                if(BRANCH_LIKELY(vec::successful(ret))) {
                    return ret + 64 * i + idx;
                }
                if (BRANCH_UNLIKELY(
                        or_if_unset(v + i, ((1UL) << idx), start_cpu))) {
                    return FAILED_RSEQ;
                }
                return 64 * i + idx;
            }
            else if (free_vecs->v[i] != EMPTY_FREE_VEC) {
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(v + i, free_vecs->v + i, start_cpu);
                if (reclaimed_slots) {
                    _atomic_xor(free_vecs->v + i, reclaimed_slots);
                    return 64 * i +
                           bits::find_first_zero<uint64_t>(reclaimed_slots);
                }
            }
        }
        return FAILED_VEC_FULL;
    }
};

template<typename objT, uint32_t nvec>
struct inner_free_vec {

    uint64_t v[nvec];

    inner _free_vec() = default;
    void ALWAYS_INLINE
    _free(const uint64_t obj_start, const uint64_t obj_loc) {
        const uint64_t obj_pos = (obj_loc - obj_start) / sizeof(objT);
        atomic_xor(v[obj_pos / 64], (1UL) << (obj_pos % 64));
    }
}


#endif
