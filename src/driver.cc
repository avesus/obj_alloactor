#include <stdio.h>
#include <stdlib.h>

#include <util/arg.h>
#include <benchmark/timer.h>
#include <system/sys_info.h>

int verbose = 0;

int
main(int argc, char ** argv) {
    PREPARE_PARSER;
    ADD_ARG("-v", "--verbose", false, Int, verbose, "Set verbosity");
    PARSE_ARGUMENTS;

    cpu::var_cpu_set<NPROCS> cpu_set(0);
    for(uint32_t i = 0; i < NPROCS; ++i) {
        cpu_set.cpu_set(i);
    }
    timer::unpinned_timer<timer::cycles_timer, NPROCS> t(10, cpu_set);
    for(uint32_t i = 0; i < 10; ++i) {
        t.take_time();
    }
    
}
