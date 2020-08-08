#ifndef _FIXED_SLAB_MANAGER_H_
#define _FIXED_SLAB_MANAGER_H_

#include <stdint.h>
#include <new>
#include <type_traits>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/sys_info.h>
#include <system/mmap_helper.h>

#include <allocator/common/internal_returns.h>
#include <allocator/rseq/rseq_base.h>

#include <allocator/slab_layout/obj_slab.h>
#include <allocator/slab_layout/super_slab.h>

#include "slab_manager_template_helpers.h"

//////////////////////////////////////////////////////////////////////
// simple slab manager that simply allocates 1 super_slab/obj_slab per
// processor. Region size must be set with template parameters

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct fixed_slab_manager;

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct internal_fixed_slab_manager {
    using slab_t =
        typename fixed_slab_manager<T, levels, per_level_nvec...>::slab_t;
    slab_t obj_slabs[NPROCS];
    internal_fixed_slab_manager() = default;
};

template<typename T, int32_t levels, int32_t... per_level_nvec>
struct fixed_slab_manager {
    using slab_t =
        typename type_helper<T, levels, 0, per_level_nvec...>::type;

    internal_fixed_slab_manager<T, levels, per_level_nvec...> * m;

    fixed_slab_manager()
        : fixed_slab_manager(mmap_alloc_noreserve(sizeof(
              internal_fixed_slab_manager<T, levels, per_level_nvec...>))) {

        // this is just to get the first page for each CPU
        // DO NOT USE ON MULTI SOCKET SYSTEM!!!!
        for (uint32_t i = 0; i < NPROC; ++i) {
            m->obj_slabs[i] = 0;
        }
    }

    fixed_slab_manager(void * const base) {
        m = (internal_fixed_slab_manager<T, levels, per_level_nvec...> *)base;
        PTR_ALIGNED_TO(m, CACHE_LINE_SIZE);
        new ((void * const)base)
            internal_fixed_slab_manager<T, levels, per_level_nvec...>();
    }

    T *
    _allocate() {
        uint64_t ptr;
        do {
            const uint32_t start_cpu = get_start_cpu();
            IMPOSSIBLE_VALUES(start_cpu > NPROCS);
            ptr = m->obj_slabs[start_cpu]._allocate(start_cpu);
        } while (BRANCH_UNLIKELY(ptr == FAILED_RSEQ));
        return (T *)(ptr & (~(0x1UL)));
    }
    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)m));
        const uint32_t from_cpu =
            (((uint64_t)addr) - ((uint64_t)m)) / sizeof(slab_t);

        IMPOSSIBLE_VALUES(from_cpu > NPROCS);
        if (from_cpu == get_start_cpu()) {
            m->obj_slabs[from_cpu]._optimistic_free(addr, from_cpu);
        }
        else {
            m->obj_slabs[from_cpu]._free(addr);
        }
    }
};


#endif
