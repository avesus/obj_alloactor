#ifndef _BENCH_UTILS_H_
#define _BENCH_UTILS_H_


#define bench_flush_all_pending() asm volatile("" : : : "memory");
#define bench_do_not_optimize_out(X) asm volatile("" : : "r,m"(X) : "memory")

#endif
