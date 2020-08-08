#ifndef _SUPER_SLAB_H_
#define _SUPER_SLAB_H_

#include <stdint.h>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include <allocator/rseq/rseq_base.h>

#include <allocator/common/internal_returns.h>
#include <allocator/common/safe_atomics.h>
#include <allocator/common/vec_constants.h>

#include "obj_slab.h"

template<typename T, uint32_t nvec = 8, typename inner_slab_t = obj_slab<T>>
struct super_slab {

    uint64_t     available_slabs[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    uint64_t     freed_slabs[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    inner_slab_t inner_slabs[64 * nvec];

    super_slab() = default;

    void
    _optimistic_free(T * const addr, const uint32_t start_cpu) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&obj_arr[0])));
        const uint64_t pos =
            (((uint64_t)addr) - ((uint64_t)(&obj_arr[0]))) / sizeof(T);
        (inner_slabs + pos)->_optimistic_free(addr, start_cpu);
        if (BRANCH_UNLIKELY(rseq_xor(available_slabs + (pos / 64),
                                     ((1UL) << (pos % 64)),
                                     start_cpu))) {
            atomic_or(freed_slabs + (pos / 64), ((1UL) << (pos % 64)));
        }
    }

    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&inner_slabs[0])));


        uint64_t pos = (((uint64_t)addr) - ((uint64_t)(&inner_slabs[0]))) /
                       sizeof(inner_slab_t);
        (inner_slabs + pos)->_free(addr);
        atomic_or(freed_slabs + (pos / 64), (1UL) << (pos % 64));
    }
    uint64_t
    _allocate(const uint32_t start_cpu) {
        for (uint32_t i = 0; i < nvec; ++i) {
            while (1) {
                while (BRANCH_LIKELY(available_slabs[i] != FULL_ALLOC_VEC)) {
                    const uint32_t idx =
                        bits::find_first_zero<uint64_t>(available_slabs[i]);

                    // we were preempted between if statement and ffz
                    if (BRANCH_UNLIKELY(idx >= 64)) {
                        return FAILED_RSEQ;
                    }

                    const uint64_t ret =
                        (inner_slabs + 64 * i + idx)->_allocate(start_cpu);
                    if (BRANCH_LIKELY(successful(
                            ret))) {  // fast path of successful allocation
                        return ret;
                    }
                    else if (ret) {  // full
                        if (or_if_unset(available_slabs + i,
                                        ((1UL) << idx),
                                        start_cpu)) {
                            return FAILED_RSEQ;
                        }
                        continue;
                    }
                    return FAILED_RSEQ;
                }
                if (freed_slabs[i] != EMPTY_FREE_VEC) {
                    const uint64_t reclaimed_slabs =
                        try_reclaim_all_free_slots(available_slabs + i,
                                                   freed_slabs + i,
                                                   start_cpu);
                    if (BRANCH_LIKELY(reclaimed_slabs)) {
                        atomic_xor(freed_slabs + i, reclaimed_slabs);
                        continue;
                    }
                    return FAILED_RSEQ;
                }
                // continues will reset, if we ever faill through to here we
                // want to stop
                break;
            }
        }
        return FAILED_VEC_FULL;
    }
};


#endif
