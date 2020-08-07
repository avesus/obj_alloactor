#ifndef _OUTER_VEC_H_
#define _OUTER_VEC_H_

#include <misc/cpp_attributes.h>
#include <system/sys_info.h>
#include <optimized/bits.h>


#include "rseq/rseq_base.h"

#include <allocator/internal_vec_returns.h>
#include <allocator/safe_atomics.h>

#include <stdint.h>

//////////////////////////////////////////////////////////////////////
// Not part of the api
template<uint32_t nvec>
struct outer_allocation_vec {
    uint64_t v[nvec];

    outer_allocation_vec() = default;

    // free_vecs and obj_start will both be computable at compile time. Since
    // this function will be inlined they should be compiled out.
    uint32_t ALWAYS_INLINE
    _allocate(const uint32_t                           start_cpu,
              uint64_t * const free_vecs) {
        for (uint32_t i = 0; i < nvec; ++i) {
            if (BRANCH_LIKELY(v[i] != FULL_ALLOC_VEC)) {
                const uint32_t idx = bits::find_first_zero<uint64_t>(v[i]);
                if (BRANCH_UNLIKELY(
                        or_if_unset(v + i, ((1UL) << idx), start_cpu))) {
                    return vecs::FAILED_RSEQ;
                }
                return 64 * i + idx;
            }
            else if (free_vecs[i] != EMPTY_FREE_VEC) {
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(v + i, free_vecs + i, start_cpu);
                if (BRANCH_LIKELY(reclaimed_slots)) {
                    _atomic_xor(free_vecs + i, reclaimed_slots);
                    return 64 * i +
                           bits::find_first_one<uint64_t>(reclaimed_slots);
                }
            }
        }
        return FAILED_VEC_FULL;
    }
};

template<typename objT, uint32_t nvec>
struct outer_free_vec {
    uint64_t v[nvec];

    outer_free_vec() = default;
    void ALWAYS_INLINE
    _free(const uint64_t obj_start, const uint64_t obj_loc) {
        const uint64_t obj_pos = (obj_loc - obj_start) / sizeof(objT);
        atomic_xor(v[obj_pos / 64], (1UL) << (obj_pos % 64));
    }
}


#endif
