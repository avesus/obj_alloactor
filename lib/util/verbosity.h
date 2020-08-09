#ifndef _VERBOSITY_H_
#define _VERBOSITY_H_


namespace verb {

static constexpr const int32_t error_verbose = 0;
static constexpr const int32_t low_verbose   = 1;
static constexpr const int32_t med_verbose   = 2;
static constexpr const int32_t high_verbose  = 3;

// define this as needed
#ifdef DISABLE_ALL_PRINTING
#define VPRINT(...)
#else
#define VPRINT(X, ...)                                                         \
    if (verbose >= (X)) {                                                      \
        fprintf(stderr, __VA_ARGS__);                                          \
    }
#endif

#define errv_print(...)  VPRINT(verb::error_verbose, __VA_ARGS__)
#define lowv_print(...)  VPRINT(verb::low_verbose, __VA_ARGS__)
#define medv_print(...)  VPRINT(verb::med_verbose, __VA_ARGS__)
#define highv_print(...) VPRINT(verb::high_verbose, __VA_ARGS__)

#ifndef DBG_PRINT
#define DBG_PRINT(...)  // define this as needed on per file bases
#endif


}  // namespace verb

int32_t verbose = verb::error_verbose;
#endif
