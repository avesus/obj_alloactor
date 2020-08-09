#ifndef _OBJ_VEC_H_
#define _OBJ_VEC_H_

#include <stdint.h>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include <allocator/rseq/rseq_base.h>

#include <allocator/common/internal_returns.h>
#include <allocator/common/safe_atomics.h>
#include <allocator/common/vec_constants.h>

#include "const_obj_vec_helpers.h"

template<typename T, uint32_t levels, uint32_t... per_level_nvec>
struct obj_vec {
    template<uint32_t n>
    using const_vals = detail::cvals<T, n, per_level_nvec...>;

    template<uint32_t n>
    ALWAYS_INLINE CONST_ATTR uint64_t *
                  get_alloc_vec(const uint32_t v_idx) const {
        return alloc_vecs + const_vals<n>::total_alloc_arr_size + v_idx;
    }

    template<uint32_t n>
    ALWAYS_INLINE CONST_ATTR uint64_t *
                  get_free_vec(const uint32_t v_idx) const {

        return free_vecs + const_vals<n>::total_free_arr_size + v_idx;
    }

    uint64_t
        alloc_vecs[const_vals<levels + 1>::total_alloc_arr_size] ALIGN_ATTR(
            CACHE_LINE_SIZE);
    uint64_t free_vecs[const_vals<levels + 1>::total_free_arr_size] ALIGN_ATTR(
        CACHE_LINE_SIZE);
    T obj[const_vals<levels + 1>::total_alloc_arr_size] ALIGN_ATTR(
        CACHE_LINE_SIZE);


    obj_vec() = default;

    uint64_t
    _allocate_final(const uint32_t vec_idx, const uint32_t start_cpu) {

        for (uint32_t i = 0; i < const_vals<levels>::get_nvecs; ++i) {
            if (BRANCH_LIKELY(get_alloc_vec<levels>(vec_idx + i)[0] !=
                              vec::FULL)) {
                const uint32_t idx = bits::find_first_zero<uint64_t>(
                    get_alloc_vec<levels>(vec_idx + i)[0]);
                if (BRANCH_UNLIKELY(
                        or_if_unset(get_alloc_vec<levels>(vec_idx + i),
                                    ((1UL) << idx),
                                    start_cpu))) {
                    return FAILED_RSEQ;
                }
                return (uint64_t)(&obj[vec_idx + 64 * i + idx]);
            }
            else if (get_free_vec<levels>(vec_idx + i)[0] != vec::EMPTY) {
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(get_alloc_vec<levels>(vec_idx + i),
                                           get_free_vec<levels>(vec_idx + i),
                                           start_cpu);
                if (reclaimed_slots) {
                    atomic_xor(get_free_vec<levels>(vec_idx + i),
                               reclaimed_slots);
                    return (uint64_t)(
                        &obj[vec_idx + 64 * i +
                             bits::find_first_zero<uint64_t>(reclaimed_slots)]);
                }
            }
        }
        return FAILED_VEC_FULL;
    }

    template<uint32_t n>
    uint64_t
    _allocate_inner(const uint32_t vec_idx, const uint32_t start_cpu) {
        for (uint32_t i = 0; i < const_vals<n>::get_nvecs; ++i) {
            while (1) {
                while (BRANCH_LIKELY(get_alloc_vec<n>(vec_idx + i)[0] !=
                                     vec::FULL)) {
                    const uint32_t idx = bits::find_first_zero<uint64_t>(
                        get_alloc_vec<n>(vec_idx + i)[0]);
                    if constexpr (n == levels - 1) {
                        const uint64_t ret = _allocate_final(
                            _get_nvecs<per_level_nvec...>(levels) *
                                (64 * (vec_idx + i) + idx),
                            start_cpu);
                        if (BRANCH_LIKELY(successful(ret))) {
                            return ret;
                        }
                        else if (failed_full(ret)) {
                            if (or_if_unset(get_alloc_vec<n>(vec_idx + i),
                                            ((1UL) << idx),
                                            start_cpu)) {
                                return FAILED_RSEQ;
                            }
                            continue;
                        }
                        return FAILED_RSEQ;
                    }
                    else {
                        const uint64_t ret = _allocate_inner<n + 1>(
                            _get_nvecs<per_level_nvec...>(n + 1) *
                                (64 * (i + vec_idx) + idx),
                            start_cpu);
                        if (BRANCH_LIKELY(successful(ret))) {
                            return ret;
                        }
                        else if (failed_full(ret)) {
                            if (or_if_unset(get_alloc_vec<n>(vec_idx + i),
                                            ((1UL) << idx),
                                            start_cpu)) {
                                return FAILED_RSEQ;
                            }
                            continue;
                        }
                        return FAILED_RSEQ;
                    }
                }
                if (get_free_vec<n>(vec_idx + i)[0] != vec::EMPTY) {
                    const uint64_t reclaimed_slabs = try_reclaim_all_free_slabs(
                        get_alloc_vec<n>(vec_idx + i),
                        get_free_vec<n>(vec_idx + i),
                        start_cpu);
                    if (BRANCH_LIKELY(reclaimed_slabs)) {
                        atomic_xor(get_free_vec<n>(vec_idx + i),
                                   reclaimed_slabs);
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
    _allocate(const uint32_t start_cpu) {
        if constexpr (levels) {
            return _allocate_inner<0>(0, start_cpu);
        }
        else {
            return _allocate_final(0, start_cpu);
        }
    }


    template<uint32_t n>
    void
    _free(const uint64_t addr_dif) {
        const uint64_t pos_idx = addr_dif / const_vals<n>::region_size;

        // this enables compiler to optimize out the math if
        // const_vals<n>::get_nvecs == 1
        IMPOSSIBLE_VALUES(pos_idx >= 64 * const_vals<n>::get_nvecs);
        if constexpr (n < levels) {
            _free<n + 1>(addr_dif);
        }
        atomic_or(get_free_vec<n>(pos_idx / 64),
                  (1UL) << (pos_idx % 64));
    }

    void
    _free(T * const addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&obj[0])));
        _free<0>((uint64_t)addr - ((uint64_t)&obj[0]));
    }


    template<uint32_t n>
    void
    _optimistic_free(const uint64_t addr_dif, const uint32_t start_cpu) {
        if constexpr (n < levels) {
            _free<n + 1>(addr_dif);
        }
        if (BRANCH_UNLIKELY(rseq_xor(
                get_alloc_vec<n>(addr_dif /
                                 _region_size<T, per_level_nvec...>(n)),
                (1UL) << (addr_dif % _region_size<T, per_level_nvec...>(n)),
                start_cpu))) {
            atomic_or(
                get_free_vec<n>(addr_dif /
                                _region_size<T, per_level_nvec...>(n)),
                (1UL) << (addr_dif % _region_size<T, per_level_nvec...>(n)));
        }
    }

    void
    _optimistic_free(T * const addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(&obj[0])));
        _optimistic_free<0>((uint64_t)addr - ((uint64_t)&obj[0]));
    }
};


#endif
