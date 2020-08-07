#ifndef _SLAB_MANAGER_H_
#define _SLAB_MANAGER_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>


#include "rseq/rseq_base.h"

#include "internal_returns.h"
#include "obj_slab.h"
#include "super_slab.h"

#include <stdint.h>

#include <new>
#include <type_traits>


template<typename... Vals>
constexpr int32_t
get_N(int32_t n, Vals... vals) {
    int32_t temp[] = { static_cast<int32_t>(vals)... };
    return temp[n];
}


template<typename T, int32_t nlevels, int32_t level, int32_t... per_level_nvec>
struct type_helper;


template<typename T, int32_t nlevel, int32_t... per_level_nvec>
struct type_helper<T, nlevel, 0, per_level_nvec...> {
    typedef slab<T, get_N(nlevel, per_level_nvec...)> type;
};

template<typename T, int32_t nlevels, int32_t level, int32_t... per_level_nvec>
struct type_helper {
    typedef super_slab<
        T,
        get_N(nlevels - level, per_level_nvec...),
        typename type_helper<T, nlevels, level - 1, per_level_nvec...>::type>
        type;
};

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct slab_manager;

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct internal_slab_manager {
    using slab_t = typename slab_manager<T, levels, per_level_nvec...>::slab_t;
    slab_t obj_slabs[NPROCS];
    internal_slab_manager() = default;
};

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct slab_manager {
    using slab_t =
        typename type_helper<T, levels - 1, levels - 1, per_level_nvec...>::
            type;
    internal_slab_manager<T, levels, per_level_nvec...> * m;

    slab_manager(const uint64_t base) {
        assert((base % 64) == 0);
        m = (internal_slab_manager<T, levels, per_level_nvec...> *)base;
        PTR_ALIGNED_TO(m, CACHE_LINE_SIZE);
        new ((void * const)base)
            internal_slab_manager<T, levels, per_level_nvec...>();
    }

    T *
    _allocate() {
        uint64_t ptr;
        do {
            const uint32_t start_cpu = get_start_cpu();
            IMPOSSIBLE_VALUES(start_cpu > NPROCS);
            ptr = m->obj_slabs[start_cpu]._allocate(start_cpu);
        } while (BRANCH_UNLIKELY(ptr == FAILED_RSEQ));
        return (T *)(BRANCH_LIKELY(successful(ptr)) ? (ptr) : NULL);
    }
    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)m));
        const uint32_t from_cpu =
            (((uint64_t)addr) - ((uint64_t)m)) / sizeof(slab_t);

        IMPOSSIBLE_VALUES(from_cpu > NPROCS);
        m->obj_slabs[from_cpu]._free(addr);
    }
};


#endif
