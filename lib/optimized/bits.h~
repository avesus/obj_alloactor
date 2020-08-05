#ifndef _BITS_H_
#define _BITS_H_
#include <stdint.h>
#include <x86intrin.h>
#include <type_traits>


#include <misc/cpp_attributes.h>


namespace bits {

template<typename T>
ALWAYS_INLINE CONST_ATTR 
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    bitcount(const T v) {
    return _mm_popcnt_u32(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    bitcount(const T v) {
    return (uint32_t)_mm_popcnt_u64(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    find_first_one(const T v) {
    return _tzcnt_u32(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    find_first_zero(const T v) {
    return _tzcnt_u32(~v);
}


// return -1 if v is 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    find_last_one(const T v) {
    return 31 - _lzcnt_u32(v);
}

// return -1 if v is ~0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    find_last_zero(const T v) {
    return 31 - _lzcnt_u32(~v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    find_first_one(const T v) {
    return (uint32_t)_tzcnt_u64(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    find_first_zero(const T v) {
    return (uint32_t)_tzcnt_u64(~v);
}

// return -1 if v is 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    find_last_one(const T v) {
    return 63 - (uint32_t)_lzcnt_u64(v);
}

// return -1 if v is ~0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    find_last_zero(const T v) {
    return 63 - (uint32_t)_lzcnt_u64(~v);
}

// returns 32 if v == 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    tzcnt(const T v) {
    return _tzcnt_u32(v);
}

// returns 32 if v == 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    tzcnt(const T v) {
    return _tzcnt_u64(v);
}

// returns 0 if v == 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    lzcnt(const T v) {
    return _lzcnt_u32(v);
}

// returns 0 if v == 0
template<typename T>
ALWAYS_INLINE CONST_ATTR
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint32_t>::type
    lzcnt(const T v) {
    return _lzcnt_u64(v);
}


// the bswap might be better off being replace with movbe asm (if it ever
// becomes important)
template<typename T>
ALWAYS_INLINE CONST_ATTR constexpr
    typename std::enable_if<(sizeof(T) < sizeof(uint32_t)), uint16_t>::type
    bswap(uint16_t v) {
    return __builtin_bswap16(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR constexpr
    typename std::enable_if<(sizeof(T) == sizeof(uint32_t)), uint32_t>::type
    bswap(const T v) {
    return __builtin_bswap32(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR constexpr
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint64_t>::type
    bswap(const T v) {
    return __builtin_bswap64(v);
}

template<typename T>
ALWAYS_INLINE CONST_ATTR constexpr
    typename std::enable_if<(sizeof(T) <= sizeof(uint32_t)), uint32_t>::type
    next_p2(const T v) {
    const uint32_t temp = bits::lzcnt<T>(v - 1);
    return temp == 0 ? 0 : ((1U) << (32 - temp));
}

template<typename T>
ALWAYS_INLINE CONST_ATTR constexpr
    typename std::enable_if<(sizeof(T) > sizeof(uint32_t)), uint64_t>::type
    next_p2(const T v) {
    const uint32_t temp = bits::lzcnt<T>(v - 1);
    return temp == 0 ? 0 : ((1UL) << (64 - temp));
}


template<typename T>
constexpr T ALWAYS_INLINE CONST_ATTR
to_mask(const uint32_t v) {
    if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return ((1UL) << v) - 1;
    }
    else {
        return (1 << v) - 1;
    }
}

template<typename T>
constexpr uint32_t ALWAYS_INLINE CONST_ATTR
nth_bit(const T v, const uint32_t n) {
    return (v >> n) & 0x1;
}

}  // namespace bits
#endif
