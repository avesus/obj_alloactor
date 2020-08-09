#include <allocator/slab_layout/dynamic_slab_manager.h>
#include <allocator/slab_layout/fixed_slab_manager.h>

#include <misc/error_handling.h>
#include <optimized/const_math.h>
#include <util/arg.h>
#include <util/verbosity.h>


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

__thread uint32_t sum = 0;

typedef void * (*tfunc_ptr)(void *);

template<typename allocator_t>
struct tester {
    allocator_t allocator;

    pthread_attr_t    attr;
    pthread_barrier_t b;
    bool              barrier_init;
    pthread_t *       tids;
    uint32_t          current_nthreads;
    uint32_t          current_capacity;

    uint64_t true_sum;
    uint64_t expected;

    uint32_t test_size;

    tester() : allocator() {
        // size expected
        assert(sizeof(allocator) == sizeof(void *));

        // ptr is valid
        assert((((uint64_t)allocator.m) % 64) == 0);

        memset(&b, 0, sizeof(pthread_barrier_t));
        barrier_init     = false;
        tids             = 0;
        current_nthreads = 0;
        current_capacity = 0;

        ERROR_ASSERT(!pthread_attr_init(&attr));

        // seriously decreases overhead of thread spawn
        pthread_attr_setstacksize(&attr, (1 << 15));
    }

    ~tester() {
        if (barrier_init) {
            pthread_barrier_destroy(&b);
        }

        pthread_attr_destroy(&attr);
        if (tids) {
            free(tids);
        }
    }

    void
    nthreads_at_func(uint32_t nthreads, void * (*tfunc)(void *)) {
        current_nthreads = nthreads;
        if (nthreads > current_capacity && tids) {
            free(tids);
            current_capacity = nthreads;
        }

        tids = (pthread_t *)calloc(nthreads, sizeof(pthread_t));
        ERROR_ASSERT(tids);

        if (barrier_init) {
            pthread_barrier_destroy(&b);
        }

        barrier_init = true;
        pthread_barrier_init(&b, NULL, nthreads);

        for (uint32_t i = 0; i < nthreads; i++) {
            // do this so we use all cores at least once if sufficient threads
            if (i < NPROCS) {
                cpu_set_t cset;
                CPU_ZERO(&cset);
                CPU_SET(i, &cset);
                pthread_attr_t _attr;
                ERROR_ASSERT(!pthread_attr_init(&_attr));
                ERROR_ASSERT(!pthread_attr_setstacksize(&_attr, (1 << 16)));
                ERROR_ASSERT(!pthread_attr_setaffinity_np(&_attr,
                                                          sizeof(cpu_set_t),
                                                          &cset));

                ERROR_ASSERT(
                    !pthread_create(tids + i, &_attr, tfunc, (void *)(this)));
            }
            else {
                ERROR_ASSERT(
                    !pthread_create(tids + i, &attr, tfunc, (void *)(this)));
            }
        }
    }

    void
    get_all_threads() {
        DIE_ASSERT(tids);
        for (uint32_t i = 0; i < current_nthreads; i++) {
            pthread_join(tids[i], NULL);
        }
    }


    void *
    alloc() {  // no argument expect implied this
        init_thread();
        expected = cmath::min<uint32_t>(
            allocator.capacity * cmath::min<uint32_t>(8, current_nthreads),
            current_nthreads * test_size);
        true_sum = 0;
        sum      = 0;

        pthread_barrier_wait(&(b));

        for (uint32_t i = 0; i < test_size; ++i) {
            uint64_t ptr = (uint64_t)allocator._allocate();
            if (ptr) {
                sum++;
            }
        }

        __atomic_fetch_add(&(true_sum), sum, __ATOMIC_RELAXED);
        return NULL;
    }


    void *
    alloc_then_free() {
        init_thread();
        expected = (current_nthreads * test_size);


        true_sum = 0;
        sum      = 0;

        pthread_barrier_wait(&(b));
        for (uint32_t i = 0; i < test_size; ++i) {
            uint64_t ptr = (uint64_t)allocator._allocate();
            if (ptr) {
                sum++;
                allocator._free((uint64_t *)ptr);
            }
        }
        __atomic_fetch_add(&(true_sum), sum, __ATOMIC_RELAXED);
        return NULL;
    }

    void *
    batch_alloc_then_free() {
        init_thread();
        expected = (current_nthreads * test_size);

        // divide by NPROCS so that in worst case scenario when all threads are
        // on same core there will always be spare memory (i.e expected will
        // always be accurate)
        const uint32_t batch_size =
            cmath::max<uint32_t>(allocator.capacity / (NPROCS * current_nthreads), 1);


        true_sum = 0;
        sum      = 0;

        uint64_t * ptrs = (uint64_t *)calloc(batch_size, sizeof(uint64_t));
        ERROR_ASSERT(ptrs);
        uint32_t   current_batch_idx = 0;
        pthread_barrier_wait(&(b));
        for (uint32_t i = 0; i < test_size; ++i) {
            uint64_t ptr = (uint64_t)allocator._allocate();
            if (ptr) {
                sum++;
                ptrs[current_batch_idx++] = ptr;
                if (current_batch_idx == batch_size) {
                    for (uint32_t _i = 0; _i < batch_size; _i++) {
                        allocator._free((uint64_t *)ptrs[_i]);
                    }
                    current_batch_idx = 0;
                }
            }
        }
        free(ptrs);
        __atomic_fetch_add(&(true_sum), sum, __ATOMIC_RELAXED);
        return NULL;
    }

    void
    run_alloc_test(uint32_t nthreads, uint32_t nalloc_per_thread) {
        lowv_print(
            "\nRunning Allocation Test\n\t"
            "NThreads        : %d\n\t"
            "Call Per Thread : %d\n",
            nthreads,
            nalloc_per_thread);

        test_size = nalloc_per_thread;

        nthreads_at_func(nthreads, (tfunc_ptr)(&tester<allocator_t>::alloc));

        get_all_threads();
        lowv_print(
            "Test Complete\n\t"
            "Expected        : %lu\n\t"
            "Received        : %lu\n",
            expected,
            true_sum);

        assert(true_sum == expected);
        allocator.reset();
    }

    void
    run_alloc_then_free_test(uint32_t nthreads, uint32_t nalloc_per_thread) {
        lowv_print(
            "\nRunning Alloc Then Free Test\n\t"
            "NThreads        : %d\n\t"
            "Call Per Thread : %d\n",
            nthreads,
            nalloc_per_thread);

        test_size = nalloc_per_thread;

        nthreads_at_func(nthreads,
                         (tfunc_ptr)(&tester<allocator_t>::alloc_then_free));

        get_all_threads();
        lowv_print(
            "Test Complete\n\t"
            "Expected        : %lu\n\t"
            "Received        : %lu\n",
            expected,
            true_sum);

        if (allocator.capacity < current_nthreads) {
            errv_print(
                "\t!!! Error: allocator capacity is not sufficient to reliably "
                "assert expected !!!\n\n");
        }
        else {
            assert(true_sum == expected);
        }
        allocator.reset();
    }

    void
    run_batch_alloc_then_free_test(uint32_t nthreads,
                                   uint32_t nalloc_per_thread) {
        lowv_print(
            "\nRunning Batch Alloc Then Free Test\n\t"
            "NThreads        : %d\n\t"
            "Call Per Thread : %d\n",
            nthreads,
            nalloc_per_thread);

        test_size = nalloc_per_thread;

        nthreads_at_func(
            nthreads,
            (tfunc_ptr)(&tester<allocator_t>::batch_alloc_then_free));

        get_all_threads();
        lowv_print(
            "Test Complete\n\t"
            "Expected        : %lu\n\t"
            "Received        : %lu\n",
            expected,
            true_sum);

        if (allocator.capacity < current_nthreads) {
            errv_print(
                "\t!!! Error: allocator capacity is not sufficient to reliably "
                "assert expected !!!\n\n");
        }
        else {
            assert(true_sum == expected);
        }
        allocator.reset();
    }

    void
    run_tests(uint32_t nthreads, uint32_t nalloc_per_thread) {
        run_alloc_then_free_test(nthreads, nalloc_per_thread);
        run_batch_alloc_then_free_test(nthreads, nalloc_per_thread);
        run_alloc_test(nthreads, nalloc_per_thread);

    }
};

uint32_t tmin = 1, tmax = 1024;
int32_t  tincr = (-1);
uint32_t tsize = (1 << 20);


int
main(int argc, char ** argv) {
    PREPARE_PARSER;
    ADD_ARG("-v", "--verbose", false, Int, verbose, "Set verbosity");
    ADD_ARG("-tmin",
            "--thread-min",
            false,
            Int,
            tmin,
            "Starting nthreads for tests");
    ADD_ARG("-tmax",
            "--thread-max",
            false,
            Int,
            tmax,
            "Ending (inclusive) nthreads for tests");
    ADD_ARG("-tincr",
            "--thread-incr",
            false,
            Int,
            tincr,
            "Incr to go up by (-1 to go by powers of 2)");
    ADD_ARG("-s", "--size", false, Int, tsize, "Test size (calls PER THREAD)");
    PARSE_ARGUMENTS;

    tester<fixed_slab_manager<uint64_t, 2, 1, 1, 2>> t;
    for (uint32_t i = tmin; i <= tmax; i += (tincr == (-1) ? i : tincr)) {
        t.run_tests(i, tsize);
    }
}
