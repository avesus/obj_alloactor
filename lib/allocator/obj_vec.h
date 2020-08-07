#ifndef _OBJ_VEC_H_
#define _OBJ_VEC_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>

#include "rseq/rseq_base.h"

#include "internal_returns.h"
#include "internal_vec_returns.h"
#include "safe_atomics.h"

#define PRINT(...) //fprintf(stderr, __VA_ARGS__)
template<uint32_t... per_level_nvecs>
static constexpr uint32_t
_get_nvecs(const uint32_t n) {
    int32_t temp[8] = { static_cast<int32_t>(per_level_nvecs)... };
    return temp[n];
}
static_assert(_get_nvecs<1, 2, 3>(0) == 1);
static_assert(_get_nvecs<1, 2, 3>(1) == 2);
static_assert(_get_nvecs<1, 2, 3>(2) == 3);
static_assert(_get_nvecs<1, 2, 3, 1>(3) == 1);

template<uint32_t... per_level_nvecs>
static constexpr uint64_t
_calculate_alloc_arr_size(const uint32_t n) {
    return n ? (64 * _get_nvecs<per_level_nvecs...>(n) *
                _calculate_alloc_arr_size<per_level_nvecs...>(n - 1))
             : _get_nvecs<per_level_nvecs...>(0);
}
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(0) == 1);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(1) == 1 * 2 * 64);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(2) == 1 * 2 * 3 * 64 * 64);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(3) ==
              1 * 2 * 3 * 4 * 64 * 64 * 64);


static constexpr uint64_t
_calculate_free_arr_size(const uint32_t n) {
    return n ? 64 * 8 * _calculate_free_arr_size(n - 1) : 8;
}
static_assert(_calculate_free_arr_size(0) == 8);
static_assert(_calculate_free_arr_size(1) == 8 * 8 * 64);
static_assert(_calculate_free_arr_size(2) == 8 * 8 * 8 * 64 * 64);
static_assert(_calculate_free_arr_size(3) == 8 * 8 * 8 * 8 * 64 * 64 * 64);

template<uint32_t... per_level_nvec>
static constexpr uint64_t
_total_alloc_arr_size(const uint32_t n) {
    return n ? _calculate_alloc_arr_size<per_level_nvec...>(n - 1) +
                   _total_alloc_arr_size<per_level_nvec...>(n - 1)
             : 0;
}
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(0) == 0);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(1) == 1);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(2) == 1 + 1 * 2 * 64);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(3) ==
              1 + 1 * 2 * 64 + 1 * 2 * 3 * 64 * 64);


static constexpr uint64_t
_total_free_arr_size(const uint32_t n) {
    return n ? _calculate_free_arr_size(n - 1) + _total_free_arr_size(n - 1)
             : 0;
}

static_assert(_total_free_arr_size(0) == 0);
static_assert(_total_free_arr_size(1) == 8);
static_assert(_total_free_arr_size(2) == 8 + 8 * 8 * 64);
static_assert(_total_free_arr_size(3) == 8 + 8 * 8 * 64 + 8 * 8 * 8 * 64 * 64);


template<typename T, uint32_t... per_level_nvec>
static constexpr uint64_t
_region_size(const uint32_t n) {
    return 64 * sizeof(T) * _calculate_alloc_arr_size<per_level_nvec...>(n);
}
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(0) ==
              1 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(1) ==
              1 * 2 * 64 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(2) ==
              1 * 2 * 3 * 64 * 64 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(3) ==
              1 * 2 * 3 * 4 * 64 * 64 * 64 * 64 * sizeof(uint64_t));


template<typename T, uint32_t levels, uint32_t... per_level_nvec>
struct obj_vec {


    template<uint32_t n>
    constexpr uint64_t *
    get_alloc_vec(const uint32_t v_idx) {
        return alloc_vecs +
            _total_alloc_arr_size<per_level_nvec...>(n) +
               v_idx;
    }

    template<uint32_t n>
    constexpr uint64_t *
    get_free_vec(const uint32_t v_idx) {
        return free_vecs + _total_free_arr_size(n) + v_idx;
    }

    uint64_t manager_vec;
    uint64_t alloc_vecs[_total_alloc_arr_size<per_level_nvec...>(levels + 1)] ALIGN_ATTR(64);
    uint64_t free_vecs[_total_free_arr_size(levels + 1)] ALIGN_ATTR(64);
    T        obj[_total_alloc_arr_size<per_level_nvec...>(levels + 1)];


    obj_vec() {
        memset(&manager_vec,
               0,
               (1 + _total_alloc_arr_size<per_level_nvec...>(levels + 1) +
                _total_free_arr_size(levels + 1)) *
                   sizeof(uint64_t));
    }

    uint64_t
    _allocate_outer(const uint32_t ovec_idx, const uint32_t start_cpu) {
        for (uint32_t i = 0; i < _get_nvecs<per_level_nvec...>(levels); ++i) {
            if (BRANCH_LIKELY(get_alloc_vec<levels>(ovec_idx + i)[0] !=
                              vec::FULL)) {
                const uint32_t idx = bits::find_first_zero<uint64_t>(
                    get_alloc_vec<levels>(ovec_idx + i)[0]);
                if (BRANCH_UNLIKELY(
                        or_if_unset(get_alloc_vec<levels>(ovec_idx + i),
                                    ((1UL) << idx),
                                    start_cpu))) {
                    return FAILED_RSEQ;
                }
                return (uint64_t)(&obj[ovec_idx + 64 * i + idx]);
            }
            else if (get_free_vec<levels>(ovec_idx + i)[0] != vec::EMPTY) {
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(get_alloc_vec<levels>(ovec_idx + i),
                                           get_free_vec<levels>(ovec_idx + i),
                                           start_cpu);
                if (reclaimed_slots) {
                    atomic_xor(get_free_vec<levels>(ovec_idx + i),
                               reclaimed_slots);
                    return (uint64_t)(
                        &obj[ovec_idx + 64 * i +
                             bits::find_first_zero<uint64_t>(reclaimed_slots)]);
                }
            }
        }
        return FAILED_VEC_FULL;
    }

    template<uint32_t n>
    uint64_t
    _allocate_inner(const uint32_t vec_idx, const uint32_t start_cpu) {
        PRINT("Allocation Inner (%d, %d, %d) -> [%d - %d]\n",
              n,
              vec_idx,
              start_cpu,
              0,
              _get_nvecs(n));
        for (uint32_t i = 0; i < _get_nvecs<per_level_nvec...>(n); ++i) {
            PRINT("\tOut of: %d / %ld\n",
                  vec_idx,
                  _calculate_alloc_arr_size<per_level_nvec...>(n));
            PRINT("\tOuter Loop [%d / %d]\n",
                  i,
                  _get_nvecs<per_level_nvec...>(n));
            PRINT("\tOuter {\n\tAvail: 0x%016lx\n\tFree : 0x%016lx\n\t}\n",
                  get_alloc_vec<n>(vec_idx + i)[0],
                  get_free_vec<n>(vec_idx + i)[0]);
            while (1) {
                while (BRANCH_LIKELY(get_alloc_vec<n>(vec_idx + i)[0] !=
                                     vec::FULL)) {
                    PRINT("\t\tInner Loop: 0x%016lx\n",
                          get_alloc_vec<n>(vec_idx + i)[0]);
                    const uint32_t idx = bits::find_first_zero<uint64_t>(
                        get_alloc_vec<n>(vec_idx + i)[0]);
                    PRINT("\t\tIdx: %d\n", idx);
                    if constexpr (n == levels - 1) {
                        PRINT("\t\tBase Case\n");
                        const uint64_t ret =
                            _allocate_outer(_get_nvecs<per_level_nvec...>(levels) * (64 * (vec_idx + i) + idx),
                                            start_cpu);
                        PRINT("\t\tRet: %lx\n", ret);
                        if (BRANCH_LIKELY(successful(
                                ret))) {  // fast path of successful allocation
                            PRINT("\t\tSUCCESSS!\n");
                            return ret;
                        }
                        else if (failed_full(ret)) {
                            PRINT("\t\tFULL!\n");
                            if (or_if_unset(get_alloc_vec<n>(vec_idx + i),
                                            ((1UL) << idx),
                                            start_cpu)) {
                                return FAILED_RSEQ;
                            }
                            continue;
                        }
                        PRINT("\t\tPREEMPTED!\n");
                        return FAILED_RSEQ;
                    }
                    else {
                        const uint64_t ret =
                            _allocate_inner<n + 1>(_get_nvecs<per_level_nvec...>(n + 1) * (64 * (i + vec_idx) + idx),
                                                   start_cpu);
                        if (BRANCH_LIKELY(successful(
                                ret))) {  // fast path of successful allocation
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
                    PRINT("\t\tInner Free: 0x%016lx\n",
                          get_free_vec<n>(vec_idx + i)[0]);
                    PRINT("\t\tBefore Avail: 0x%016lx\n",
                          get_alloc_vec<n>(vec_idx + i)[0]);
                    const uint64_t reclaimed_slabs = try_reclaim_all_free_slots(
                        get_alloc_vec<n>(vec_idx + i),
                        get_free_vec<n>(vec_idx + i),
                        start_cpu);
                    PRINT("\t\tAfter Avail: 0x%016lx\n",
                          get_alloc_vec<n>(vec_idx + i)[0]);
                    PRINT("\t\tReclaimed: %lx\n", reclaimed_slabs);
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
            return _allocate_outer(0, start_cpu);
        }
    }


    template<uint32_t n>
    void
    _free(const uint64_t addr_dif) {
        PRINT("Start Free: %d %p + (%lu / %lu)\n",
              n,
              get_free_vec<n>(0),
              addr_dif,
              _region_size<T, per_level_nvec...>(n));
        if constexpr (n < levels) {
            _free<n + 1>(addr_dif / _region_size<T, per_level_nvec...>(n));
            atomic_or(
                get_free_vec<n>(addr_dif /
                                _region_size<T, per_level_nvec...>(n)),
                (1UL) << (addr_dif % _region_size<T, per_level_nvec...>(n)));
        }
        else {

            atomic_or(
                get_free_vec<n>(addr_dif /
                                _region_size<T, per_level_nvec...>(n)),
                (1UL) << (addr_dif % _region_size<T, n, per_level_nvec...>(n)));
        }
        PRINT("Done Free: %d\n", n);
    }

    void
    _free(T * const addr) {
        _free<0>((uint64_t)addr - ((uint64_t)&obj[0]));
    }
};


#endif
