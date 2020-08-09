#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include <allocator/rseq/rseq_ops.h>

#define TEST_SIZE ((1UL) << 19)
#define NTHREAD   (4096UL)

pthread_barrier_t b;

uint64_t inline __attribute__((always_inline)) __attribute__((const))
get_cycles() {
    uint32_t hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}

uint64_t
ts_to_ns(struct timespec * ts) {
    return 1000UL * 1000UL * 1000UL * ts->tv_sec + ts->tv_nsec;
}

uint64_t
ts_ns_dif(struct timespec * ts_end, struct timespec * ts_start) {
    return ts_to_ns(ts_end) - ts_to_ns(ts_start);
}

uint64_t true_time_cycles = 0;
uint64_t true_time_ns     = 0;


void *
test_add(void * targ) {
    uint64_t * arr = (uint64_t *)targ;

    struct timespec ts_start, ts_end;
    uint64_t        start, end;
    
    init_thread();
    pthread_barrier_wait(&b);
    
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    start = get_cycles();

    for (uint64_t i = 0; i < TEST_SIZE; ++i) {
        //        uint32_t start_cpu = get_start_cpu();;
        const uint32_t start_cpu = get_start_cpu();
        acquire_lock(arr + 8 * start_cpu, start_cpu);
            //  start_cpu = get_start_cpu();;
    }

    end = get_cycles();
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    __atomic_fetch_add(&true_time_ns,
                       ts_ns_dif(&ts_end, &ts_start),
                       __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_cycles, end - start, __ATOMIC_RELAXED);
    return NULL;
}

int
main() {


    pthread_barrier_init(&b, NULL, NTHREAD);

    uint64_t * arr = (uint64_t *)calloc(8, 64);

    pthread_t tids[NTHREAD];

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (1 << 16));
    pthread_attr_setguardsize(&attr, 0);

    for (uint64_t i = 0; i < NTHREAD; i++) {
        pthread_create(tids + i, &attr, test_add, (void *)(arr));
    }

    for (uint64_t i = 0; i < NTHREAD; i++) {
        pthread_join(tids[i], NULL);
    }

    fprintf(stderr, "%s ->\n\t", "Test Add\n");
    fprintf(stderr, "Threads = %ld, Num_Incr = %ld\n\t", NTHREAD, TEST_SIZE);
    fprintf(stderr,
            "\tCycles       : %.4E\n\t",
            (double)(true_time_cycles / NTHREAD));
    fprintf(stderr,
            "\tCycles Per Op: %lu\n\t",
            (true_time_cycles / NTHREAD) / ((uint64_t)TEST_SIZE));
    fprintf(stderr,
            "\tnsec         : %.4E\n\t",
            (double)(true_time_ns / NTHREAD));
    fprintf(stderr,
            "\tnsec Per Op  : %lu\n\t",
            (true_time_ns / NTHREAD) / ((uint64_t)TEST_SIZE));

    uint64_t sum = 0;
    for (uint32_t i = 0; i < 8; i++) {
        sum += arr[8 * i];
    }
    fprintf(stderr, "%lu == %lu\n", sum, TEST_SIZE * NTHREAD);
    assert(sum == TEST_SIZE * NTHREAD);
}
