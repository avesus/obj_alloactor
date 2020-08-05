#ifndef _TIMER_H_
#define _TIMER_H_

#include <sched.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>


#include <concurrency/cpu.h>
#include <misc/error_handling.h>
#include <misc/cpp_attributes.h>
#include <optimized/const_math.h>
#include <system/mmap_helpers.h>
#include <system/sys_info.h>

namespace timer {

static constexpr int32_t  UNKOWN_NUMBER      = (-1);
static constexpr uint64_t DEFAULT_MAX_MEMORY = ((1UL) << 30);
static constexpr uint32_t DEFAULT_INIT_PAGES = 4;


template<typename time_taker>
class pinned_timer {
    using timer_type = typename time_taker::timer_type;

    time_taker   t;
    timer_type * times;
    timer_type * times_ptr_begin;

   public:
    pinned_timer(int32_t ntimes) {
        this->times = (timer_type *)mmap_alloc_pagein_reserve(
            ntimes ? (ntimes * sizeof(timer_type)) : DEFAULT_MAX_MEMORY,
            ntimes ? cmath::roundup_div<uint64_t>(ntimes * sizeof(timer_type),
                                                  PAGE_SIZE)
                   : DEFAULT_INIT_PAGES);
    }

    constexpr void ALWAYS_INLINE
    take_time() {
        t((this->times));
        ++(this->times);
    }
};


template<typename time_taker, uint32_t NCPU>
class unpinned_timer {
    using timer_type = typename time_taker::timer_type;
    //    using get_cpu_func = typename cpu::get_cpu_function_template;
    using cpu_set_iterator = typename cpu::var_cpu_set<NCPU>::cpu_set_iterator;
    using d_cpu_set_iterator = typename cpu::var_cpu_set<NCPU>::d_cpu_set_iterator;
    //    get_cpu_func vdso_get_cpu;
    time_taker   t;
    timer_type * times[NCPU];
    timer_type * times_ptr_begin[NCPU];


   public:
    // idea is with cheap call to get cpu you will get more precise timing by
    // binding a memory region to core for each CPU. Easy enough (nway merge
    // basically) to recombine all the ptrs
    unpinned_timer(int32_t ntimes, cpu::var_cpu_set<NCPU> & cpu_set) {
        cpu::init_get_cur_cpu();
        d_cpu_set_iterator it(cpu_set);
        cpu::var_cpu_set<NCPU> temp;
        temp.cpu_zero();
        uint32_t cur_cpu = ++it;
        for (; it != cpu_set.dend(); cur_cpu = ++it) {
            temp.cpu_set(cur_cpu);

            ERROR_ASSERT(
                sched_setaffinity(0,
                                  temp.get_size(), 
                                  temp.get_cpu_set_t()) == 0,
                "Set affinity to cpu(%d) failed\n",
                cur_cpu);  //(const cpu_set_t *)(&_cpu_set));
            while(cpu::get_cur_cpu() != cur_cpu) {
                usleep(50);
            }

            this->times[cur_cpu] = (timer_type *)mmap_alloc_pagein_reserve(
                ntimes ? (ntimes * sizeof(timer_type)) : DEFAULT_MAX_MEMORY,
                ntimes
                    ? cmath::roundup_div<uint64_t>(ntimes * sizeof(timer_type),
                                                   PAGE_SIZE)
                    : DEFAULT_INIT_PAGES);

            times_ptr_begin[cur_cpu] = times[cur_cpu];
            temp.cpu_isset_clr(cur_cpu);
        }
    }


    constexpr void ALWAYS_INLINE
    take_time_new() {
        const uint32_t cpu = cpu::get_cur_cpu_init();
        t((this->times[cpu]));
        ++(this->times[cpu]);
    }

    constexpr void ALWAYS_INLINE
    take_time() {
        const uint32_t cpu = cpu::get_cur_cpu();
        t((this->times[cpu]));
        ++(this->times[cpu]);
    }
};


struct ts_timer {

    using timer_type = struct timespec;

    void ALWAYS_INLINE CONST_ATTR
    operator()(timer_type * location) const {
        clock_gettime(CLOCK_MONOTONIC, location);
    }
};

struct cycles_timer {

    using timer_type = uint64_t;

    // need to specify attribute const as opposed to c++ const because inline
    // assembly
    void ALWAYS_INLINE PURE_ATTR
    operator()(timer_type * location) {
        uint32_t hi, lo;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        *location = (((uint64_t)lo) | (((uint64_t)hi) << 32));
    }
};

}  // namespace timer

#endif
