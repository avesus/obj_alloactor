#ifndef _ERROR_HANDLING_H_
#define _ERROR_HANDLING_H_

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <misc/cpp_attributes.h>
// Error* versions are if errno will have been set.
// Die* versions are if not errno related

namespace ERR {


#define ERROR_ASSERT(X, msg, args...)                                          \
    if (BRANCH_UNLIKELY(!(X))) {                                               \
        ERROR_DIE(msg, ##args);                                                \
    }

#define DIE_ASSERT(X, msg, args...)                                            \
    if (BRANCH_UNLIKELY(!(X))) {                                               \
        DIE(msg, ##args);                                                      \
    }

#define DIE(msg, args...)                                                      \
    ERR::die(__FILE__, __LINE__, msg, ##args);                                 \
    _UNREACHABLE_;

#define ERROR_DIE(msg, args...)                                                \
    ERR::errdie(__FILE__, __LINE__, errno, msg, ##args);                       \
    _UNREACHABLE_;


void COLD_ATTR NEVER_INLINE
errdie(const char * file_name,
       uint32_t     line_number,
       int32_t      error_number,
       const char * msg,
       ...) {
    va_list ap;
    va_start(ap, msg);
    fprintf(stderr, "%s:%d: ", file_name, line_number);
    vfprintf(stderr, msg,
             ap);  // NOLINT /* This warning is a clang-tidy bug */
    va_end(ap);
    fprintf(stderr, "\t%d:%s\n", error_number, strerror(error_number));
    exit(-1);
}

void COLD_ATTR NEVER_INLINE
die(const char * file_name, uint32_t line_number, const char * msg, ...) {
    va_list ap;
    va_start(ap, msg);
    fprintf(stderr, "%s:%d: ", file_name, line_number);
    vfprintf(stderr, msg,
             ap);  // NOLINT /* This warning is a clang-tidy bug */
    va_end(ap);
    exit(-1);
}

}  // namespace ERR
#endif
