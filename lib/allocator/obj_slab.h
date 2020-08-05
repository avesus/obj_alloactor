#ifndef _OBJ_SLAB_H_
#define _OBJ_SLAB_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include "rseq/rseq_base.h"

#include "internal_returns.h"
#include "safe_atomics.h"

#include <stdint.h>


template<typename T, uint32_t nvec = 8>
struct slab {

    uint64_t available_slots[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    uint64_t freed_slots[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);
    T        obj_arr[64 * nvec];


    slab() = default;


    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&obj_arr[0])));

        uint64_t pos =
            (((uint64_t)addr) - ((uint64_t)(&obj_arr[0]))) / sizeof(T);

        atomic_or(freed_slots + (pos / 64), ((1UL) << (pos % 64)));
    }

    uint64_t
    _allocate(const uint32_t start_cpu) {
        for (uint32_t i = 0; i < nvec; ++i) {
            // try allocate
            if (BRANCH_LIKELY(
                              available_slots[i] != FULL_ALLOC_VEC)) {
                const uint32_t idx =
                    bits::find_first_zero<uint64_t>(available_slots[i]);
                if (BRANCH_UNLIKELY(or_if_unset(available_slots + i,
                                                ((1UL) << idx),
                                                start_cpu))) {
                    return FAILED_RSEQ;
                }
                return ((uint64_t)&obj_arr[64 * i + idx]);
            }
            // try free
            else if (freed_slots[i] != EMPTY_FREE_VEC) {
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(available_slots + i,
                                           freed_slots + i,
                                           start_cpu);
                if (BRANCH_LIKELY(reclaimed_slots)) {
                    atomic_xor(freed_slots + i, reclaimed_slots);

                    return ((uint64_t)(
                        &obj_arr[64 * i + bits::find_first_one<uint64_t>(
                                              reclaimed_slots)]));
                }
                else {
                    return FAILED_RSEQ;
                }
            }
        }
        return FAILED_VEC_FULL;
    }
};

#endif
