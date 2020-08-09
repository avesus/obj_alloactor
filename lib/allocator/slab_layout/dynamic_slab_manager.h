#ifndef _DYNAMIC_SLAB_MANAGER_H_
#define _DYNAMIC_SLAB_MANAGER_H_

#include <stdint.h>
#include <new>
#include <type_traits>

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
#include <optimized/const_math.h>
#include <system/mmap_helpers.h>
#include <system/sys_info.h>

#include <allocator/common/internal_returns.h>
#include <allocator/rseq/rseq_base.h>

#include <allocator/slab_layout/obj_slab.h>
#include <allocator/slab_layout/super_slab.h>

#include "slab_manager_template_helpers.h"

template<reclaim_policy rp = reclaim_policy::SHARED>
struct cpu_region {
    static constexpr const uint32_t nfree_vec =
        rp == reclaim_policy::PERCPU ? 8 * NPROCS : 1;

    uint64_t allocable_regions ALIGN_ATTR(CACHE_LINE_SIZE);
    uint64_t                   free_regions_lock;

    uint64_t free_regions[nfree_vec] ALIGN_ATTR(CACHE_LINE_SIZE);
    cpu_region() = default;
};

template<reclaim_policy rp = reclaim_policy::SHARED>
struct region_manager {
    static constexpr const uint32_t cpu_map_bits =
        cmath::ulog2<uint32_t>(NPROCS);
    static constexpr const uint32_t cpu_map_bits_div = (64 / cpu_map_bits);

    cpu_region<rp> percpu_regions[NPROCS] ALIGN_ATTR(CACHE_LINE_SIZE);

    // a cache line to itself because this will have the most contention
    uint64_t available_regions ALIGN_ATTR(CACHE_LINE_SIZE);

    // this should waste no memory as we are cache aligning slabs
    uint64_t region_map[cmath::roundup<uint32_t>(cpu_map_bits, 8)] ALIGN_ATTR(
        CACHE_LINE_SIZE);


    region_manager() = default;

    uint32_t ALWAYS_INLINE
    add_new_region(const uint32_t start_cpu) {
        const uint32_t new_region_idx =
            __atomic_fetch_add(&available_regions, 1, __ATOMIC_RELAXED);
        // setup cpu map (this is used for free to go from memory location ->
        // cpu owner)
        atomic_or(
            region_map + (new_region_idx / cpu_map_bits_div),
            start_cpu << (cpu_map_bits * (new_region_idx % cpu_map_bits_div)));

        if (BRANCH_UNLIKELY(
                rseq_or(&(percpu_regions[start_cpu].allocable_regions),
                        (1UL) << new_region_idx,
                        start_cpu))) {
            // slow path add to free region. We are not going to be using this
            // immediately anyways and probably best to let next alloc on this
            // CPU get the vector
            atomic_or(&(percpu_regions[start_cpu].free_regions),
                      (1UL) << new_region_idx);
            return WAS_PREEMPTED;
        }
        return new_region_idx;
    }


    uint32_t ALWAYS_INLINE
    get_region(const uint32_t start_cpu) {
        // fast path there are available regions
        if (BRANCH_LIKELY(percpu_regions[start_cpu].allocable_regions)) {
            return bits::find_first_one(
                percpu_regions[start_cpu].allocable_regions);
        }


        if constexpr (rp == reclaim_policy::SHARED) {
#ifdef SAFER_FREE
            if (BRANCH_UNLIKELY(acquire_lock(
                    &(percpu_regions[start_cpu].freed_regions_lock),
                    start_cpu))) {
                return WAS_PREEMPTED;
            }

#ifdef CONTINUE_RSEQ
            // this option allows for preemption between check on
            // availabe_regions and acquiring the lock
            if (percpu_regions[start_cpu].freed_regions[0]) {
                const uint64_t reclaimed_regions = try_reclaim_all_free_slabs(
                    &(percpu_regions[start_cpu].allocable_regions),
                    percpu_regions[start_cpu].freed_regions,
                    start_cpu);
                if (BRANCH_LIKELY(reclaimed_regions)) {
                    atomic_xor(percpu_regions[start_cpu].freed_regions,
                               reclaimed_regions);
                    percpu_regions[start_cpu].freed_regions_lock = 0;
                    return bits::find_first_one(reclaimed_regions);
                }
                percpu_regions[start_cpu].freed_regions_lock = 0;
                return WAS_PREEMPTED;
            }
#endif
#ifndef CONTINUE_RSEQ
            if (BRANCH_UNLIKELY(percpu_regions[start_cpu].allocable_regions)) {
                percpu_regions[start_cpu].freed_regions_lock = 0;
                return WAS_PREEMPTED;
            }

            if (percpu_regions[start_cpu].freed_regions_lock[0]) {
                const uint64_t reclaimed_regions =
                    percpu_regions[start_cpu].freed_regions_lock[0];
                percpu_regions[start_cpu].allocable_regions = reclaimed_regions;
                atomic_xor(percpu_regions[start_cpu].freed_regions,
                           reclaimed_regions);
                percpu_regions[start_cpu].freed_regions_lock = 0;
                return bits::find_first_one(reclaimed_regions);
            }
#endif
            percpu_regions[start_cpu].freed_regions_lock = 0;
#endif
#ifndef SAFER_FREE
            if (percpu_regions[start_cpu].freed_regions[0]) {
                const uint64_t reclaimed_regions = try_reclaim_all_free_slabs(
                    &(percpu_regions[start_cpu].allocable_regions),
                    percpu_regions[start_cpu].freed_regions,
                    start_cpu);
                if (BRANCH_LIKELY(reclaimed_regions)) {
                    atomic_xor(percpu_regions[start_cpu].freed_regions,
                               reclaimed_regions);
                    return bits::find_first_one(reclaimed_regions);
                }
                return WAS_PREEMPTED;
            }
#endif
        }
        // reclaim_policy == PERCPU
        else {
#ifdef SAFER_FREE
            if (BRANCH_UNLIKELY(acquire_lock(
                    &(percpu_regions[start_cpu].freed_regions_lock),
                    start_cpu))) {
                return WAS_PREEMPTED;
            }
#ifdef CONTINUE_RSEQ
            for (uint32_t _i = 0; _i < 8 * NPROCS; _i += 8) {
                if (percpu_regions[start_cpu].freed_regions[_i]) {
                    const uint64_t reclaimed_regions =
                        try_reclaim_all_free_slabs(
                            &(percpu_regions[start_cpu].allocable_regions),
                            percpu_regions[start_cpu].freed_regions + _i,
                            start_cpu);
                    if (BRANCH_LIKELY(reclaimed_regions)) {
                        atomic_xor(percpu_regions[start_cpu].freed_regions + _i,
                                   reclaimed_regions);
                        percpu_regions[start_cpu].freed_regions_lock = 0;
                        return bits::find_first_one(reclaimed_regions);
                    }
                    percpu_regions[start_cpu].freed_regions_lock = 0;
                    return WAS_PREEMPTED;
                }
            }
#endif
#ifndef CONTINUE_RSEQ
            if (BRANCH_UNLIKELY(percpu_regions[start_cpu].allocable_regions)) {
                percpu_regions[start_cpu].freed_regions_lock = 0;
                return WAS_PREEMPTED;
            }
            // I think since we don't need to worry about being preempted
            // its best to get them all
            uint64_t reclaimed_regions =
                percpu_regions[start_cpu].freed_regions[0];
            if (reclaimed_regions) {
                atomic_xor(percpu_regions[start_cpu].freed_regions,
                           reclaimed_regions);
            }
            for (uint32_t _i = 8; _i < 8 * NPROCS; _i += 8) {
                const uint64_t _reclaimed_regions =
                    percpu_regions[start_cpu].freed_regions[_i];
                if (_reclaimed_regions) {
                    atomic_xor(percpu_regions[start_cpu].freed_regions + _i,
                               _reclaimed_regions);
                    reclaimed_regions |= _reclaimed_regions;
                }

            }
            if (reclaimed_regions) {
                percpu_regions[start_cpu].allocable_regions = reclaimed_regions;
                percpu_regions[start_cpu].freed_regions_lock = 0;
                return bits::find_first_one(reclaimed_regions);
            }
#endif
            percpu_regions[start_cpu].freed_regions_lock = 0;
#endif
#ifndef SAFER_FREE
            // this a very expensive loop due to the fact that all
            // other freed_slabs are "owned" by other CPUS
            for (uint32_t _i = 0; _i < 8 * NPROCS; _i += 8) {
                if (percpu_regions[start_cpu].freed_regions[_i]) {
                    const uint64_t reclaimed_regions =
                        try_reclaim_all_free_slabs(
                            &(percpu_regions[start_cpu].allocable_regions),
                            percpu_regions[start_cpu].freed_regions + _i,
                            start_cpu);
                    if (BRANCH_LIKELY(reclaimed_regions)) {
                        atomic_xor(percpu_regions[start_cpu].freed_regions + _i,
                                   reclaimed_regions);
                        return bits::find_first_one(reclaimed_regions);
                    }
                    return WAS_PREEMPTED;
                }
            }
#endif
        }
        return add_new_region(start_cpu);
    }

    uint32_t ALWAYS_INLINE
    get_address_owner(const uint32_t region_idx) {
        return (region_map[region_idx / cpu_map_bits_div] >>
                (cpu_map_bits * (region_idx % cpu_map_bits_div))) &
               (cpu_map_bits - 1);
    }

    void ALWAYS_INLINE
    try_mark_non_allocable(const uint64_t region_mask,
                           const uint32_t start_cpu) {
        xor_if_set(&(percpu_regions[start_cpu].allocable_regions),
                   region_mask,
                   start_cpu);
    }

    void ALWAYS_INLINE
    mark_free(const uint64_t region_mask, const uint32_t start_cpu) {
        if (start_cpu == get_start_cpu()) {
            if (BRANCH_LIKELY(
                    rseq_or(&(percpu_regions[start_cpu].allocable_regions),
                            region_mask,
                            start_cpu))) {
                return;
            }
        }
        if constexpr (rp == reclaim_policy::SHARED) {
            atomic_or(percpu_regions[start_cpu].free_regions, region_mask);
        }
        else {
            while (BRANCH_UNLIKELY(
                rseq_any_cpu_or(percpu_regions[start_cpu].free_regions,
                                region_mask)))
                ;
        }
    }
};


template<typename T,
         int32_t        levels,
         reclaim_policy rp = reclaim_policy::SHARED,
         int32_t... per_level_nvec>
struct dynamic_slab_manager {
    static constexpr const uint32_t ABSOLUTE_MAX_REGIONS = 64;

    using slab_t = typename type_helper<T, levels, 0, per_level_nvec...>::type;

    constexpr uint32_t
    _capacity(uint32_t n) const {
        return 64 * get_N<per_level_nvec...>(n) * (n ? _capacity(n - 1) : 1);
    }
    static constexpr const uint32_t capacity = _capacity(levels);


    region_manager<rp> * m;
    uint32_t max_regions;  // this might be better placed upper bits of m


    dynamic_slab_manager(const uint32_t _max_regions = ABSOLUTE_MAX_REGIONS)
        : dynamic_slab_manager(
              mmap_alloc_noreserve(
                  sizeof(region_manager<rp>) +
                  cmath::min<uint32_t>(_max_regions, ABSOLUTE_MAX_REGIONS) *
                      sizeof(slab_t)),
              cmath::min<uint32_t>(_max_regions, ABSOLUTE_MAX_REGIONS)) {}


    dynamic_slab_manager(void * const   base,
                         const uint32_t _max_regions = ABSOLUTE_MAX_REGIONS) {
        m           = (region_manager<rp> *)base;
        max_regions = _max_regions;
    }

    T *
    _allocate() {
        uint64_t ptr;
        do {
            const uint32_t start_cpu = get_start_cpu();
            const uint32_t region    = m->get_region(start_cpu);
            if (BRANCH_UNLIKELY(region >= max_regions)) {
                if (region == WAS_PREEMPTED) {
                    continue;
                }
                else {
                    return NULL;
                }
            }
            ptr = (((slab_t *)(m + 1)) + region)->_allocate(start_cpu);
            if (ptr == FAILED_VEC_FULL) {
                m->try_mark_non_allocable((1UL) << region, start_cpu);
                continue;
            }
        } while (BRANCH_UNLIKELY(ptr == FAILED_RSEQ));
    }

    void
    _free(T * addr) {
        IMPOSSIBLE_VALUES(((uint64_t)addr) < ((uint64_t)(m + 1)));
        const uint32_t region_idx = ((uint64_t)addr) - ((uint64_t)(m + 1));
        const uint32_t owner_cpu  = m->get_address_owner(region_idx);
        if (owner_cpu == get_start_cpu()) {
            (((slab_t *)(m + 1)) + region_idx)
                ->_optimistic_free(addr, owner_cpu);
        }
        else {
            (((slab_t *)(m + 1)) + region_idx)->_free(addr);
        }
        m->mark_free((1UL) << region_idx, owner_cpu);
    }
};

#endif
