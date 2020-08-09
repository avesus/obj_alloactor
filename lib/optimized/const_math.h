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
constexpr uint32_t ALWAYS_INLINE CONST_ATTR
ulog2(T v) {
    uint32_t r = 0, s = 0;
    if constexpr (sizeof(T) == sizeof(uint64_t)) {
        r = (v > 0xffffffff) << 5;
        v >>= r;
        s = (v > 0xffff) << 4;
        v >>= s;
        r |= s;
    }
    else {
        r = (v > 0xffff) << 4;
        v >>= r;
    }
    s = (v > 0xff) << 3;
    v >>= s;
    r |= s;
    s = (v > 0xf) << 2;
    v >>= s;
    r |= s;
    s = (v > 0x3) << 1;
    v >>= s;
    r |= s;
    return r | (v >> 1);
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
