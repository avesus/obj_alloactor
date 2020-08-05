#ifndef _CONST_MATH_H_
#define _CONST_MATH_H_

#include <misc/cpp_attributes.h>
#include <optimized/bits.h>
namespace cmath {

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
min(const T a, const T b) {
    return a < b ? a : b;
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
max(const T a, const T b) {
    return a > b ? a : b;
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
roundup(const T a, const T b) {
    return b * ((a + (b - 1)) / b);
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
rounddown(const T a, const T b) {
    return b * (a / b);
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
roundup_div(const T a, const T b) {
    return (a + (b - 1)) / b;
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
fast_mod(const T v, const T m) {
    // this is somehow faster than straightup % operator
    return (v < m) ? v : (v % m);
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
next_p2(T v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(v) == sizeof(uint64_t)) {
        v |= v >> 32;
    }
    v++;
    return v;
}

template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
prev_p2(T v) {
    v >>= 1;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if constexpr (sizeof(v) == sizeof(uint64_t)) {
        v |= v >> 32;
    }
    v++;
    return v;
}


}  // namespace cmath

#endif
