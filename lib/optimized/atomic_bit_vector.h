#ifndef _ATOMIC_BIT_VECTOR_H_
#define _ATOMIC_BIT_VECTOR_H_

#include <misc/cpp_attributes.h>
#include <util/atomic_utils.h>
#include <util/const_utils.h>

namespace vatm {
enum atomic_bvec_modes { set_1s = 0, set_0s = 1 };
using mem_order = int32_t;

template<atomics::safety s = safe, atomic_bvec_modes mode = set_1s>
struct atomic_bit_vector {

    using internal_T = atomics::atomic_t<s>::type;

    static constexpr int32_t
    bit_scan_failure() const {
        if constexpr {
            return cutil::sizeof_bits<uint64_t>();
        }
        else {
            return (-1);
        }
    }

    constexpr atomic_bit_vector(const uint64_t seed) {
        vec = seed;
    }

    ~atomic_bit_vector() = default;
    constexpr atomic_bit_vector() {
        if constexpr (mode == set_1s) {
            vec = 0;
        }
        else {
            vec = ~(0UL);
        }
    }
    constexpr atomic_bit_vector(const uint64_t _vec) : vec(_vec) {}
    constexpr atomic_bit_vector(const atomic_bit_vector other)
        : vec(other.vec) {}


    constexpr uint32_t ALWAYS_INLINE
    find() const {
        if constexpr (mode == set_1s) {
            return bits::find_first_one<uint64_t>(vec);
        }
        else {
            return bits::find_first_zero<uint64_t>(vec);
        }
    }

    constexpr uint32_t ALWAYS_INLINE
    set_inner(const uint64_t pos, mem_order m = __ATOMIC_RELAXED) {
        // idea is this returns 0 if the fetch was 0 (i.e that means this call
        // set the bit), otherwise returns 1
        if constexpr (mode == set_1s) {
            // if this or didnt set then then pos & result will be 1
            return __builtin_atomic_fetch_or(&vec, pos, m) & pos;
        }
        else {
            // if this and didnt set then bit at pos will be 0 (there may be a
            // way to do this will xor)
            return !(__builtin_atomic_fetch_and(&vec, ~pos, m) & pos);
        }
    }

    constexpr uint32_t ALWAYS_INLINE
    set(mem_order m = __ATOMIC_RELAXED) {
        int32_t pos;
        do {
            pos = this->find();
            if (BRANCH_UNLIKELY(pos == bit_scan_failure())) {
                return bit_scan_failure();
            }
        } while (BRANK_UNLIKELY(set_inner((1UL) << pos, m)));
        return pos;
    }

    constexpr void ALWAYS_INLINE
    unset(const uint32_t pos, mem_order m = __ATOMIC_RELAXED) {
        __builtin_atomic_fetch_xor(&vec, (1UL) << pos, m);
    }
};

}  // namespace vatm
#endif
