#ifndef _SAFE_ATOMICS_H_
#define _SAFE_ATOMICS_H_

#include <misc/cpp_attributes.h>
#include <stdint.h>


// just wrapper for __atomic_xor_fetch that has no return value (to avoid
// accidentally using it and converting to CAS)
constexpr void ALWAYS_INLINE
atomic_xor(uint64_t * const v_loc, const uint64_t xor_bits) {
    __atomic_fetch_xor(v_loc, xor_bits, __ATOMIC_RELAXED);
}

constexpr void ALWAYS_INLINE
atomic_unset(uint64_t * const v_loc, const uint64_t unset_bits) {
    __atomic_fetch_and(v_loc, ~unset_bits, __ATOMIC_RELAXED);
}

constexpr void ALWAYS_INLINE
atomic_or(uint64_t * const v_loc, const uint64_t xor_bits) {
    __atomic_fetch_or(v_loc, xor_bits, __ATOMIC_RELAXED);
}


#endif
