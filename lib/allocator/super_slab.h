#ifndef _SUPER_SLAB_H_
#define _SUPER_SLAB_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include "rseq/rseq_base.h"

#include "internal_returns.h"
#include "obj_slab.h"
#include "safe_atomics.h"


template<typename T,
         uint32_t nvec         = 8,
         uint32_t inner_nvec   = 8,
         typename inner_slab_t = slab<T, inner_nvec>>
struct super_slab {
    uint64_t     available_slabs[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    uint64_t     freed_slabs[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    inner_slab_t inner_slabs[64 * nvec / inner_nvec];

    super_slab() = default;

    uint32_t
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&inner_slabs[0])));


        uint64_t pos = (((uint64_t)addr) - ((uint64_t)(&inner_slabs[0]))) /
                       sizeof(inner_slab_t);
        const uint32_t inner_i = (inner_slabs + pos)->_free(addr);
        atomic_or(freed_slabs + (pos / (64 / inner_nvec)),
                  (1UL) << (((64 / inner_nvec) * inner_i) +
                            (pos % (64 / inner_nvec))));
        return pos / (64 / inner_nvec);
    }
    uint64_t
    _allocate(const uint32_t start_cpu) {
        for (uint32_t i = 0; i < nvec; ++i) {
            while (1) {
                while (BRANCH_LIKELY(available_slabs[i] != FULL_ALLOC_VEC)) {
                    const uint32_t idx =
                        bits::find_first_zero<uint64_t>(available_slabs[i]);
                    const uint64_t ret =
                        (inner_slabs + (64 / inner_nvec) * i +
                         (idx / inner_nvec))
                            ->_allocate(start_cpu, idx % inner_nvec);
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

    uint64_t
    _allocate(const uint32_t start_cpu, const uint32_t i) {
        while (1) {
            while (BRANCH_LIKELY(available_slabs[i] != FULL_ALLOC_VEC)) {
                const uint32_t idx =
                    bits::find_first_zero<uint64_t>(available_slabs[i]);
                const uint64_t ret =
                    (inner_slabs + (64 / inner_nvec) * i + (idx / inner_nvec))
                        ->_allocate(start_cpu, idx % inner_nvec);
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
            return FAILED_VEC_FULL;
        }

    }
};

#endif
