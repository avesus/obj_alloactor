#ifndef _FIXED_SLAB_MANAGER_H_
#define _FIXED_SLAB_MANAGER_H_

#include <stdint.h>
#include <new>
#include <type_traits>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <system/mmap_helpers.h>
#include <system/sys_info.h>

#include <allocator/common/internal_returns.h>
#include <allocator/rseq/rseq_base.h>

#include <allocator/slab_layout/obj_slab.h>
#include <allocator/slab_layout/super_slab.h>

#include "slab_manager_template_helpers.h"

//////////////////////////////////////////////////////////////////////
// simple slab manager that simply allocates 1 super_slab/obj_slab per
// processor. Region size must be set with template parameters

template<typename T, uint32_t levels, uint32_t... per_level_nvec>
struct fixed_slab_manager;

template<typename T, uint32_t levels, uint32_t... per_level_nvec>
struct internal_fixed_slab_manager {
    using slab_t =
        typename fixed_slab_manager<T, levels, per_level_nvec...>::slab_t;
    slab_t obj_slabs[NPROCS];
    internal_fixed_slab_manager() = default;
};

template<typename T, uint32_t levels, uint32_t... per_level_nvec>
struct fixed_slab_manager {
    using slab_t = typename type_helper<T, levels, 0, per_level_nvec...>::type;

    static constexpr uint32_t
    _capacity(uint32_t n) {
        return 64 * get_N<per_level_nvec...>(n) * (n ? _capacity(n - 1) : 1);
    }
    static constexpr const uint32_t capacity = _capacity(levels);

    using internal_manager_t =
        internal_fixed_slab_manager<T, levels, per_level_nvec...>;
    
    internal_manager_t * m;

    fixed_slab_manager()
        : fixed_slab_manager(mmap_alloc_noreserve(sizeof(internal_manager_t))) {

        // this is just to get the first page for each CPU
        // DO NOT USE ON MULTI SOCKET SYSTEM!!!!
        for (uint32_t i = 0; i < NPROCS; ++i) {
            *((uint64_t *)(m->obj_slabs + i)) = 0;
        }
    }

    fixed_slab_manager(void * const base) {
        m = (internal_manager_t *)base;
        new ((void * const)base) internal_manager_t();
    }

    ~fixed_slab_manager() {
        safe_munmap(m, sizeof(internal_manager_t));
    }

    void
    reset() {
        memset(m, 0, sizeof(internal_manager_t));
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
