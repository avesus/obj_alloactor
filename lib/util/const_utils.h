#ifndef _CONST_UTILS_H_
#define _CONST_UTILS_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
namespace cutil {

template<typename T>
constexpr uint32_t ALWAYS_INLINE CONST_ATTR
type_size_log() {
    return cmath::ulog2<decltype(sizeof(T))>(
        cmath::next_p2<decltype(sizeof(T))>(sizeof(T)));
}


template<typename T>
constexpr uint32_t ALWAYS_INLINE CONST_ATTR
sizeof_bits() {
    return 8 * sizeof(T);
}


}  // namespace cutil


#endif
