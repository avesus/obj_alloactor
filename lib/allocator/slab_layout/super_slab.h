#ifndef _SUPER_SLAB_H_
#define _SUPER_SLAB_H_

#include <stdint.h>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include <util/verbosity.h>


#include <allocator/rseq/rseq_base.h>

#include <allocator/common/internal_returns.h>
#include <allocator/common/safe_atomics.h>
#include <allocator/common/vec_constants.h>

#include "obj_slab.h"
#include "slab_config.h"


template<typename T,
         uint32_t nvec         = 7,
         typename inner_slab_t = obj_slab<T>,
         reclaim_policy rp     = reclaim_policy::SHARED>
struct super_slab {
    static constexpr const uint32_t nfree_vec =
        rp == reclaim_policy::PERCPU ? 8 * NPROCS : nvec;

    uint64_t available_slabs[nvec] ALIGN_ATTR(CACHE_LINE_SIZE);

#ifdef SAFER_FREE
    // the race condition which makes this necessasry in obj_slab.h while
    // present in super_slabs would not affect correctness as a non available
    // obj_slab being set as available will not cause duplicate allocation and
    // instead will only cause a wasted "FAILED_VEC_FULL" return
    uint64_t freed_slabs_lock;
#endif

    uint64_t     freed_slabs[nfree_vec] ALIGN_ATTR(CACHE_LINE_SIZE);
    inner_slab_t inner_slabs[64 * nvec] ALIGN_ATTR(CACHE_LINE_SIZE);

    super_slab() = default;


    void
    _optimistic_free(T * const addr, const uint32_t start_cpu) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&inner_slabs[0])));

        const uint64_t pos_idx =
            (((uint64_t)addr) - ((uint64_t)(&inner_slabs[0]))) /
            sizeof(inner_slab_t);

        // this enables compiler to optimize out the math if nvec == 1
        IMPOSSIBLE_VALUES(pos_idx >= nvec * 64);

        (inner_slabs + pos_idx)->_optimistic_free(addr, start_cpu);
        if (BRANCH_UNLIKELY(rseq_and(available_slabs + (pos_idx / 64),
                                     ~((1UL) << (pos_idx % 64)),
                                     start_cpu))) {

            if constexpr (rp == reclaim_policy::SHARED) {
                atomic_or(freed_slabs + (pos_idx / 64),
                          ((1UL) << (pos_idx % 64)));
            }
            else {
                while (BRANCH_UNLIKELY(
                    rseq_any_cpu_or(freed_slabs, (1UL) << (pos_idx % 64))))
                    ;
            }
        }
    }

    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&inner_slabs[0])));


        const uint64_t pos_idx =
            (((uint64_t)addr) - ((uint64_t)(&inner_slabs[0]))) /
            sizeof(inner_slab_t);

        IMPOSSIBLE_VALUES(pos_idx >= nvec * 64);

        (inner_slabs + pos_idx)->_free(addr);
        if constexpr (rp == reclaim_policy::SHARED) {
            atomic_or(freed_slabs + (pos_idx / 64), (1UL) << (pos_idx % 64));
        }
        else {
            while (BRANCH_UNLIKELY(
                rseq_any_cpu_or(freed_slabs, (1UL) << (pos_idx % 64))))
                ;
        }
    }

    uint64_t
    _allocate(const uint32_t start_cpu) {
        for (uint32_t i = 0; i < nvec; ++i) {
            while (1) {
                DBG_PRINT(
                    "OUTER_LOOP\n\t"
                    "i            : %d\n\t"
                    "avail_slab   : 0x%016lx\n\t"
                    "free_slab    : 0x%016lx\n",
                    i,
                    available_slabs[i],
                    freed_slabs[i]);
                while (BRANCH_LIKELY(available_slabs[i] != vec::FULL)) {
                    const uint32_t idx =
                        bits::find_first_zero<uint64_t>(available_slabs[i]);
                    DBG_PRINT(
                        "INNER_LOOP\n\t"
                        "idx          : %d\n",
                        idx);
                    // we were preempted between if statement and ffz
                    if (BRANCH_UNLIKELY(idx == 64)) {
                        DBG_PRINT("FAILED IDX == 64\n");
                        return FAILED_RSEQ;
                    }

                    const uint64_t ret =
                        (inner_slabs + 64 * i + idx)->_allocate(start_cpu);

                    DBG_PRINT(
                        "RETURN RECEIVED\n\t"
                        "ret          : %lx\n",
                        ret);
                    if (BRANCH_LIKELY(successful(
                            ret))) {  // fast path of successful allocation
                        DBG_PRINT(
                            "FAST PATH COMPLETE\n\t"
                            "i            : %d\n\t"
                            "avail_slabs  : 0x%016lx\n",
                            i,
                            available_slabs[i]);
                        return ret;
                    }
                    else if (ret) {  // full
                        DBG_PRINT(
                            "RETURNED FULL\n\t"
                            "i          : %d\n\t"
                            "idx        : %d\n\t"
                            "true idx   : %d\n",
                            i,
                            idx,
                            64 * i + idx);
                        DBG_PRINT(
                            "UNSETTING\n\t"
                            "avail_slabs      : 0x%016lx\n",
                            available_slabs[i]);
                        if (or_if_unset(available_slabs + i,
                                        ((1UL) << idx),
                                        start_cpu)) {
                            DBG_PRINT("FAILED TO UNSET\n\n");
                            return FAILED_RSEQ;
                        }
                        DBG_PRINT("UNSET - CONTINUING\n");
                        continue;
                    }
                    DBG_PRINT("WAS PREEMPTED\n");
                    return FAILED_RSEQ;
                }
                if constexpr (rp == reclaim_policy::SHARED) {
                    DBG_PRINT("TRYING TO POP FREE\n");
#ifdef SAFER_FREE
                    if (BRANCH_UNLIKELY(
                            acquire_lock(&freed_slabs_lock, start_cpu))) {
                        return FAILED_RSEQ;
                    }

#ifdef CONTINUE_RSEQ
                    // this option allows for preemption between check on
                    // availabe_slabs and acquiring the lock
                    if (freed_slabs[i] != vec::EMPTY) {
                        const uint64_t reclaimed_slabs =
                            try_reclaim_all_free_slabs(available_slabs + i,
                                                       freed_slabs + i,
                                                       start_cpu);
                        if (BRANCH_LIKELY(reclaimed_slabs)) {
                            atomic_xor(freed_slabs + i, reclaimed_slabs);
                            freed_slabs_lock = 0;
                            continue;
                        }
                        freed_slabs_lock = 0;
                        return FAILED_RSEQ;
                    }
#endif

#ifndef CONTINUE_RSEQ
                    // this option does not allow for the above preemption and
                    // thus can use slightly cheeaper operations for setting
                    // (because available_slabs is full so no other thread can
                    // write to it)

                    if (BRANCH_UNLIKELY(available_slabs[i] != vec::FULL)) {
                        freed_slabs_lock = 0;
                        return FAILED_RSEQ;
                    }

                    if (freed_slabs[i] != vec::EMPTY) {
                        const uint64_t reclaimed_slabs = freed_slabs[i];
                        available_slabs[i]             = reclaimed_slabs;
                        atomic_xor(freed_slabs + i, reclaimed_slabs);
                    }
#endif
                    freed_slabs_lock = 0;
#endif
#ifndef SAFER_FREE
                    if (freed_slabs[i] != vec::EMPTY) {
                        const uint64_t reclaimed_slabs =
                            try_reclaim_all_free_slabs(available_slabs + i,
                                                       freed_slabs + i,
                                                       start_cpu);
                        if (BRANCH_LIKELY(reclaimed_slabs)) {
                            atomic_xor(freed_slabs + i, reclaimed_slabs);
                            continue;
                        }
                        return FAILED_RSEQ;
                    }
#endif
                }
                // reclaim_policy == PERCPU
                else {
#ifdef SAFER_FREE
                    if (BRANCH_UNLIKELY(
                            acquire_lock(&freed_slabs_lock, start_cpu))) {
                        return FAILED_RSEQ;
                    }
#ifdef CONTINUE_RSEQ
                    for (uint32_t _i = 0; _i < 8 * NPROCS; _i += 8) {
                        if (freed_slabs[_i + i] != vec::EMPTY) {
                            const uint64_t reclaimed_slabs =
                                try_reclaim_all_free_slabs(available_slabs + i,
                                                           freed_slabs + _i + i,
                                                           start_cpu);
                            if (BRANCH_LIKELY(reclaimed_slabs)) {
                                atomic_xor(freed_slabs + _i + i,
                                           reclaimed_slabs);
                                break;
                            }
                            freed_slabs_lock = 0;
                            return FAILED_RSEQ;
                        }
                    }
#endif
#ifndef CONTINUE_RSEQ
                    if (BRANCH_UNLIKELY(available_slabs[i] != vec::FULL)) {
                        freed_slabs_lock = 0;
                        return FAILED_RSEQ;
                    }

                    // this saves a mov instruction :/
                    uint64_t reclaimed_slabs = freed_slabs[i];
                    if (reclaimed_slabs) {
                        atomic_xor(freed_slabs + i, reclaimed_slabs);
                    }

                    // I think since we don't need to worry about being
                    // preempted its best to get them all
                    for (uint32_t _i = 8; _i < 8 * NPROCS; _i += 8) {
                        const uint64_t _reclaimed_slabs = freed_slabs[_i + i];
                        if (_reclaimed_slabs) {
                            atomic_xor(freed_slabs + _i + i, _reclaimed_slabs);
                            reclaimed_slabs |= _reclaimed_slabs;
                        }
                    }
                    available_slabs[i] = reclaimed_slabs;

#endif
                    freed_slabs_lock = 0;
#endif
#ifndef SAFER_FREE
                    // this a very expensive loop due to the fact that all
                    // other freed_slabs are "owned" by other CPUS
                    for (uint32_t _i = 0; _i < 8 * NPROCS; _i += 8) {
                        if (freed_slabs[_i + i] != vec::EMPTY) {
                            const uint64_t reclaimed_slabs =
                                try_reclaim_all_free_slabs(available_slabs + i,
                                                           freed_slabs + _i + i,
                                                           start_cpu);
                            if (BRANCH_LIKELY(reclaimed_slabs)) {
                                atomic_xor(freed_slabs + _i + i,
                                           reclaimed_slabs);
                                break;
                            }
                            return FAILED_RSEQ;
                        }
                    }
#endif
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
