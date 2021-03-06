#ifndef _PACKED_PTR_H_
#define _PACKED_PTR_H_

// possible todo: overload all atomic ops so can take advantage of x86 relaxed

#include <misc/cpp_attributes.h>
#include <optimized/const_math.h>
#include <stdio.h>

#include <util/atomic_utils.h>
#include <util/const_utils.h>

#include <concurrency/mixed_atomic.h>

#include <atomic>

using mem_order = int32_t;
template<typename T,
         uint32_t low_bits  = cutil::type_size_log<T>(),
         uint32_t high_bits = 16,  // default space left after vm addr range
         atomics::safety s  = atomics::safe,
         mem_order       m_default = __ATOMIC_RELAXED>
struct atomic_packed_ptr;

template<typename T,
         uint32_t low_bits  = cutil::type_size_log<T>(),
         uint32_t high_bits = 16  // default space left after vm addr range
         >
struct packed_ptr {
    //////////////////////////////////////////////////////////////////////
    // this struct is meant to be high performance. as a result there is no
    // error checking inside of this struct. For example passing a low bit that
    // exceeds threshold will almost definetly cause issues.

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint32_t>::type
        high_bits_shift() noexcept {
        return cutil::sizeof_bits<uint64_t>() - high_bits;
    }

    template<uint32_t _low_bits = low_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        low_bits_mask() noexcept {
        return ((1UL) << low_bits) - 1;
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        high_bits_mask() noexcept {
        return (((1UL) << high_bits) - 1) << (high_bits_shift());
    }

    static constexpr uint64_t ALWAYS_INLINE CONST_ATTR
    ptr_mask() noexcept {
        if constexpr (low_bits && high_bits) {
            return ~(high_bits_mask() | low_bits_mask());
        }
        else if constexpr (low_bits && (!high_bits)) {
            return ~(low_bits_mask());
        }
        else if constexpr (high_bits && (!low_bits)) {
            return ~(high_bits_mask());
        }
        else {
            return (~(0UL));
        }
    }

    uint64_t ptr;

    packed_ptr() noexcept  = default;
    ~packed_ptr() noexcept = default;


    constexpr ALWAYS_INLINE
    packed_ptr(const packed_ptr<T, low_bits, high_bits> & seed_other) noexcept
        : ptr(seed_other.ptr) {}

    constexpr ALWAYS_INLINE
    packed_ptr(
        const atomic_packed_ptr<T, low_bits, high_bits> & seed_other) noexcept
        : ptr(seed_other._load()) {}

    constexpr ALWAYS_INLINE
    packed_ptr(const uint64_t seed_uint64_t) noexcept
        : ptr(seed_uint64_t) {}

    constexpr ALWAYS_INLINE
    packed_ptr(T * const seed_ptr) noexcept
        : ptr((uint64_t)seed_ptr) {}


    static constexpr ALWAYS_INLINE CONST_ATTR T *
                                              to_ptr(const uint64_t v) noexcept {
        if constexpr (low_bits || high_bits) {
            return (T *)(v & ptr_mask());
        }
        else {
            return (T *)v;
        }
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        to_not_ptr(const uint64_t v) noexcept {
        return v & (high_bits_mask() | low_bits_mask());
    }

    template<uint32_t _low_bits = low_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        to_low_bits(const uint64_t v) noexcept {
        return v & low_bits_mask();
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        to_high_bits(const uint64_t v) noexcept {
        return v >> high_bits_shift();
    }


    //////////////////////////////////////////////////////////////////////
    // Ptr helpers
    constexpr ALWAYS_INLINE PURE_ATTR T *
                                      get_ptr() const noexcept {
        return to_ptr(this->ptr);
    }

    constexpr uint64_t ALWAYS_INLINE PURE_ATTR
    get_not_ptr() const noexcept {
        if constexpr (high_bits || low_bits) {
            return to_not_ptr(this->ptr);
        }
        else {
            return 0;
        }
    }

    constexpr ALWAYS_INLINE T *
                            set_ptr(T * const new_ptr) noexcept {
        if constexpr (low_bits || high_bits) {
            return to_ptr(
                (this->ptr = ((uint64_t)new_ptr) | this->get_not_ptr()));
        }
        else {
            return this->ptr = (uint64_t)new_ptr;
        }
    }


    constexpr bool ALWAYS_INLINE PURE_ATTR
    ptr_equals(T * const other_ptr) const noexcept {
        return (this->get_ptr() == other_ptr);
    }

    constexpr ALWAYS_INLINE T *
                            add_eq_ptr(const uint64_t plus_ptr) noexcept {
        return to_ptr(this->ptr += (sizeof(T) * plus_ptr));
    }

    constexpr ALWAYS_INLINE T *
                            sub_eq_ptr(const uint64_t minus_ptr) noexcept {
        return to_ptr(this->ptr -= (sizeof(T) * minus_ptr));
    }


    constexpr ALWAYS_INLINE T *
                            incr_eq_ptr() noexcept {
        return to_ptr(this->ptr += sizeof(T));
    }

    constexpr ALWAYS_INLINE T *
                            decr_eq_ptr() noexcept {
        return to_ptr(this->ptr -= sizeof(T));
    }

    constexpr ALWAYS_INLINE PURE_ATTR T *
                                      add_ptr(const uint64_t plus_ptr) const noexcept {
        return this->get_ptr() + plus_ptr;
    }

    constexpr ALWAYS_INLINE PURE_ATTR T *
                                      sub_ptr(const uint64_t sub_ptr) const noexcept {
        return this->get_ptr() - sub_ptr;
    }


    //////////////////////////////////////////////////////////////////////
    // low bit helpers
    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        get_low_bits() const noexcept {
        return to_low_bits(this->ptr);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        get_not_low_bits() const noexcept {
        return this->ptr & (~low_bits_mask());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    set_low_bits(const uint64_t new_low_bits) noexcept {
        return to_low_bits(this->ptr = new_low_bits | this->get_not_low_bits());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, bool>::type
        low_bits_equals(const uint64_t other_low_bits) const noexcept {
        return to_low_bits(this->get_low_bits() == other_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    add_eq_low_bits(const uint64_t plus_low_bits) noexcept {
        return to_low_bits(this->ptr += plus_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    sub_eq_low_bits(const uint64_t minus_low_bits) noexcept {
        return to_low_bits(this->ptr -= minus_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    incr_eq_low_bits() noexcept {
        return to_low_bits(++(this->ptr));
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    decr_eq_low_bits() noexcept {
        return to_low_bits(--(this->ptr));
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    and_eq_low_bits(const uint64_t and_low_bits) noexcept {
        return to_low_bits(this->ptr &= (and_low_bits | (~low_bits_mask())));
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    or_eq_low_bits(const uint64_t or_low_bits) noexcept {
        return to_low_bits(this->ptr |= or_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    xor_eq_low_bits(const uint64_t xor_low_bits) noexcept {
        return to_low_bits(this->ptr ^= xor_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    nand_eq_low_bits(const uint64_t nand_low_bits) noexcept {
        return to_low_bits(this->ptr &= (~nand_low_bits));
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    shl_eq_low_bits(const uint64_t shl_low_bits) noexcept {
        this->set_low_bits(this->get_low_bits() << shl_low_bits);
        return (this->get_low_bits());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    shr_eq_low_bits(const uint64_t shr_low_bits) noexcept {
        this->set_low_bits(this->get_low_bits() >> shr_low_bits);
        return (this->get_low_bits());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        mult_eq_low_bits(const uint64_t mult_low_bits) const noexcept {
        this->set_low_bits(this->get_low_bits() * mult_low_bits);
        return (this->get_low_bits());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        div_eq_low_bits(const uint64_t div_low_bits) const noexcept {
        this->set_low_bits(this->get_low_bits() / mult_low_bits);
        return (this->get_low_bits());
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        add_low_bits(const uint64_t plus_low_bits) const noexcept {
        return (this->get_low_bits() + plus_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        sub_low_bits(const uint64_t minus_low_bits) const noexcept {
        return (this->get_low_bits() - minus_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        and_low_bits(const uint64_t and_low_bits) const noexcept {
        return (this->ptr & and_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        or_low_bits(const uint64_t or_low_bits) const noexcept {
        return (this->get_low_bits() | or_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        xor_low_bits(const uint64_t xor_low_bits) const noexcept {
        return (this->get_low_bits() ^ xor_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        nand_low_bits(const uint64_t nand_low_bits) const noexcept {
        return (this->get_low_bits() & (~nand_low_bits));
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        shl_low_bits(const uint64_t shl_low_bits) const noexcept {
        return (this->get_low_bits() << shl_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        shr_low_bits(const uint64_t shr_low_bits) const noexcept {
        return (this->get_low_bits() >> shr_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        mult_low_bits(const uint64_t mult_low_bits) const noexcept {
        return (this->get_low_bits() * mult_low_bits);
    }

    template<uint32_t _low_bits = low_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        div_low_bits(const uint64_t div_low_bits) const noexcept {
        return (this->get_low_bits() / div_low_bits);
    }


    //////////////////////////////////////////////////////////////////////
    // high bit helpers. For inplace versions (i.e if you want to explicitly add
    // (1 << high_bits_shift()) directly instead of adding (1) and having the
    // add_high_bits function shift just use += operator
    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_high_bits() const noexcept {
        return to_high_bits(this->ptr);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_high_bits_inplace() const noexcept {
        return ((this->ptr & high_bits_mask()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_not_high_bits() const noexcept {
        return (this->ptr & (~high_bits_mask()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    set_high_bits(const uint64_t new_high_bits) noexcept {
        return to_high_bits(this->ptr = (new_high_bits << high_bits_shift()) |
                                        this->get_not_high_bits());
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, bool>::type
        high_bits_equals(const uint64_t other_high_bits) const noexcept {
        return (this->get_high_bits() == other_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    add_eq_high_bits(const uint64_t plus_high_bits) noexcept {
        return to_high_bits(this->ptr += (plus_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    sub_eq_high_bits(const uint64_t minus_high_bits) noexcept {
        return to_high_bits(this->ptr -=
                            (minus_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    incr_eq_high_bits() noexcept {
        return to_high_bits(this->ptr += ((1UL) << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    decr_eq_high_bits() noexcept {
        return to_high_bits(this->ptr -= ((1UL) << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    and_eq_high_bits(const uint64_t and_high_bits) noexcept {
        return to_high_bits(this->ptr &= ((and_high_bits << high_bits_shift()) |
                                          (~high_bits_mask())));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    or_eq_high_bits(const uint64_t or_high_bits) noexcept {
        return to_high_bits(this->ptr |= (or_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    xor_eq_high_bits(const uint64_t xor_high_bits) noexcept {
        return to_high_bits(this->ptr ^= (xor_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    nand_eq_high_bits(const uint64_t nand_high_bits) noexcept {
        return to_high_bits(this->ptr &=
                            (~(nand_high_bits << high_bits_shift())));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    shl_eq_high_bits(const uint64_t shl_high_bits) noexcept {
        this->ptr = this->get_not_high_bits() |
                    (this->get_high_bits_inplace() << shl_high_bits);
        return (this->get_high_bits());
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, uint64_t>::type
    shr_eq_high_bits(const uint64_t shr_high_bits) noexcept {
        this->ptr = this->get_not_high_bits() |
                    (this->get_high_bits_inplace() >> shr_high_bits);
        return (this->get_high_bits());
    }


    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        mult_eq_high_bits(const uint64_t mult_high_bits) const noexcept {
        this->ptr = (this->get_high_bits_inplace() * mult_high_bits) |
                    this->get_not_high_bits();
        return (this->get_high_bits());
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        div_eq_high_bits(const uint64_t div_high_bits) const noexcept {
        this->set_high_bits(this->get_high_bits() / mult_high_bits);
        return (this->get_high_bits());
    }


    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        add_high_bits(const uint64_t plus_high_bits) const noexcept {
        return (this->get_high_bits() + plus_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        sub_high_bits(const uint64_t minus_high_bits) const noexcept {
        return (this->get_high_bits() - minus_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        and_high_bits(const uint64_t and_high_bits) const noexcept {
        return (this->get_high_bits() & and_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        or_high_bits(const uint64_t or_high_bits) const noexcept {
        return (this->get_high_bits() | or_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        xor_high_bits(const uint64_t xor_high_bits) const noexcept {
        return (this->get_high_bits() ^ xor_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        nand_high_bits(const uint64_t nand_high_bits) const noexcept {
        return (this->get_high_bits() & (~nand_high_bits));
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        shl_high_bits(const uint64_t shl_high_bits) const noexcept {
        return (this->get_high_bits() << shl_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        shr_high_bits(const uint64_t shr_high_bits) const noexcept {
        return (this->get_high_bits() >> shr_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        mult_high_bits(const uint64_t mult_high_bits) const noexcept {
        return (this->get_high_bits() * mult_high_bits);
    }

    template<uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        div_high_bits(const uint64_t div_high_bits) const noexcept {
        return (this->get_high_bits() / mult_high_bits);
    }

    //////////////////////////////////////////////////////////////////////
    // operator acting on entire ptr
    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(const packed_ptr<T, low_bits, high_bits> other) const noexcept {
        return (this->ptr == other.ptr);
    }

    constexpr bool ALWAYS_INLINE
    operator==(const atomic_packed_ptr<T, low_bits, high_bits> & other) const
        noexcept {
        return (this->ptr == other._load());
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(const uint64_t other) const noexcept {
        return (this->ptr == other);
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator!=(const packed_ptr<T, low_bits, high_bits> other) const noexcept {
        return (this->ptr != other.ptr);
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator!=(const uint64_t other) const noexcept {
        return (this->ptr != other);
    }

    constexpr uint64_t ALWAYS_INLINE
    operator=(const packed_ptr<T, low_bits, high_bits> seed_other) noexcept {
        return this->ptr = seed_other.ptr;
    }

    constexpr void ALWAYS_INLINE
    operator=(const uint64_t seed_uint64_t) noexcept {
        this->ptr = seed_uint64_t;
    }

    constexpr void ALWAYS_INLINE
    operator=(T * const seed_ptr) noexcept {
        this->ptr = (uint64_t)seed_ptr;
    }

    //////////////////////////////////////////////////////////////////////
    // operators acting on the pointer
    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(T * const other_ptr) const noexcept {
        return (this->ptr_equals(other_ptr));
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator!=(T * const other_ptr) const noexcept {
        return (!this->ptr_equals(other_ptr));
    }

    constexpr uint64_t ALWAYS_INLINE
    operator+=(const uint64_t plus_ptr) noexcept {
        return (this->add_eq_ptr(plus_ptr));
    }

    constexpr uint64_t ALWAYS_INLINE
    operator-=(const uint64_t minus_ptr) noexcept {
        return (this->sub_eq_ptr(minus_ptr));
    }

    constexpr ALWAYS_INLINE T *
                            operator++() noexcept {
        return (this->incr_eq_ptr());
    }

    constexpr ALWAYS_INLINE T *
                            operator--() noexcept {
        return (this->decr_eq_ptr());
    }

    constexpr ALWAYS_INLINE T *
                            operator++(int) noexcept {
        if constexpr (low_bits || high_bits) {
            return (this->incr_eq_ptr() - 1);
        }
        else {
            return ((this->ptr)++);
        }
    }

    constexpr ALWAYS_INLINE T *
                            operator--(int) noexcept {
        if constexpr (low_bits || high_bits) {
            return (this->decr_eq_ptr() + 1);
        }
        else {
            return ((this->ptr)--);
        }
    }

    constexpr ALWAYS_INLINE T & operator*() noexcept {
        return (*(this->get_ptr()));
    }

    constexpr ALWAYS_INLINE PURE_ATTR const T & operator*() const noexcept {
        return (*(this->get_ptr()));
    }

    constexpr ALWAYS_INLINE PURE_ATTR T * operator->() noexcept {
        return (this->get_ptr());
    }

    constexpr ALWAYS_INLINE PURE_ATTR const T * operator->() const noexcept {
        return (this->get_ptr());
    }

    constexpr ALWAYS_INLINE PURE_ATTR T & operator[](
        const uint32_t i) noexcept {
        return *(this->get_ptr() + i);
    }

    constexpr ALWAYS_INLINE PURE_ATTR const T & operator[](
        const uint32_t i) const noexcept {
        return *(this->get_ptr() + i);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator&=(const uint64_t and_all_bits) noexcept {
        return (this->ptr &= and_all_bits);
    }
    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator&=(const uint64_t and_all_bits) noexcept {
        return (T *)(this->ptr &= and_all_bits);
    }


    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator|=(const uint64_t or_all_bits) noexcept {
        return (this->ptr |= or_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator|=(const uint64_t or_all_bits) noexcept {
        return (T *)(this->ptr |= or_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator^=(const uint64_t xor_all_bits) noexcept {
        return (this->ptr ^= xor_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator^=(const uint64_t xor_all_bits) noexcept {
        return (T *)(this->ptr ^= xor_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator&(const uint64_t and_all_bits) const noexcept {
        return (this->ptr & and_all_bits);
    }
    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator&(const uint64_t and_all_bits) const noexcept {
        return (T *)(this->ptr & and_all_bits);
    }


    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator|(const uint64_t or_all_bits) const noexcept {
        return (this->ptr | or_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator|(const uint64_t or_all_bits) const noexcept {
        return (T *)(this->ptr | or_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        operator^(const uint64_t xor_all_bits) const noexcept {
        return (this->ptr ^ xor_all_bits);
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<!(_low_bits || _high_bits), T *>::type
        operator^(const uint64_t xor_all_bits) const noexcept {
        return (T *)(this->ptr ^ xor_all_bits);
    }
};


//////////////////////////////////////////////////////////////////////
// A key difference between this atomic vs non atomic version is generally the
// atomic version will return the OLD state of the packed ptr (as that
// information is not recoverable)
template<typename T,
         uint32_t        low_bits,
         uint32_t        high_bits,
         atomics::safety s,
         mem_order       m_default>
struct atomic_packed_ptr {
    //////////////////////////////////////////////////////////////////////
    // this struct is meant to be high performance. as a result there is no
    // error checking inside of this struct. For example passing a low bit that
    // exceeds threshold will almost definetly cause issues.

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint32_t>::type
        high_bits_shift() noexcept {
        return cutil::sizeof_bits<uint64_t>() - high_bits;
    }

    template<uint32_t _low_bits = low_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        low_bits_mask() noexcept {
        return ((1UL) << low_bits) - 1;
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        high_bits_mask() noexcept {
        return (((1UL) << high_bits) - 1) << (high_bits_shift());
    }

    static constexpr uint64_t ALWAYS_INLINE CONST_ATTR
    ptr_mask() noexcept {
        if constexpr (low_bits && high_bits) {
            return ~(high_bits_mask() | low_bits_mask());
        }
        else if constexpr (low_bits && (!high_bits)) {
            return ~(low_bits_mask());
        }
        else if constexpr (high_bits && (!low_bits)) {
            return ~(high_bits_mask());
        }
        else {
            return (~(0UL));
        }
    }

    matm::c_atomic<uint64_t, s, m_default> ptr;

    atomic_packed_ptr() noexcept  = default;
    ~atomic_packed_ptr() noexcept = default;


    constexpr ALWAYS_INLINE
    atomic_packed_ptr(
        const atomic_packed_ptr<T, low_bits, high_bits> & seed_other) noexcept
        : ptr(seed_other._load()) {}

    constexpr ALWAYS_INLINE
    atomic_packed_ptr(
        const packed_ptr<T, low_bits, high_bits> & seed_other) noexcept
        : ptr(seed_other.ptr) {}

    constexpr ALWAYS_INLINE
    atomic_packed_ptr(const uint64_t seed_uint64_t) noexcept
        : ptr(seed_uint64_t) {}

    constexpr ALWAYS_INLINE
    atomic_packed_ptr(T * const seed_ptr) noexcept
        : ptr((uint64_t)seed_ptr) {}


    static constexpr ALWAYS_INLINE T *
                                   to_ptr(const uint64_t v) noexcept {
        if constexpr (low_bits || high_bits) {
            return (T *)(v & ptr_mask());
        }
        else {
            return (T *)v;
        }
    }

    template<uint32_t _low_bits = low_bits, uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits || _high_bits, uint64_t>::type
        to_not_ptr(const uint64_t v) noexcept {
        return v & (high_bits_mask() | low_bits_mask());
    }


    template<uint32_t _low_bits = low_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        to_low_bits(const uint64_t v) noexcept {
        return v & low_bits_mask();
    }

    template<uint32_t _low_bits = low_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        to_not_low_bits(const uint64_t v) noexcept {
        return v & (~low_bits_mask());
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        to_high_bits(const uint64_t v) noexcept {
        return v >> high_bits_shift();
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        to_high_bits_inplace(const uint64_t v) noexcept {
        return v & high_bits_mask();
    }

    template<uint32_t _high_bits = high_bits>
    static constexpr ALWAYS_INLINE CONST_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        to_not_high_bits(const uint64_t v) noexcept {
        return v & (~high_bits_mask());
    }

    template<mem_order m = m_default>
    constexpr uint64_t ALWAYS_INLINE PURE_ATTR
    _load() const noexcept {
        if constexpr (m == __ATOMIC_RELAXED) {
            return this->ptr.load();
        }
        else {
            return this->ptr.load(m);
        }
    }

    template<mem_order m = m_default>
    constexpr void ALWAYS_INLINE
    _store(const uint64_t _v) const noexcept {
        if constexpr (m == __ATOMIC_RELAXED) {
            return this->ptr.store(_v);
        }
        else {
            return this->ptr.load(_v, m);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // Ptr helpers
    template<mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR T *
                                      get_ptr() const noexcept {
        return to_ptr(this->_load<m>());
    }


    template<mem_order m = m_default>
    constexpr uint64_t ALWAYS_INLINE PURE_ATTR
    get_not_ptr() const noexcept {
        if constexpr (low_bits || high_bits) {
            return to_not_ptr(this->_load<m>());
        }
        else {
            return 0;
        }
    }


    template<mem_order m1 = m_default, mem_order m2 = m_default>
    constexpr ALWAYS_INLINE packed_ptr<T, low_bits, high_bits>
                            set_ptr(T * const new_ptr) noexcept {
        if constexpr (low_bits || high_bits) {
            uint64_t expected = this->_load<m1>();
            while (!(this->ptr.compare_exchange_weak(
                expected,
                to_not_ptr(expected) | ((uint64_t)new_ptr),
                m1,
                m2)))
                ;
            return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
        }
        else {
            // choosing to return new val here because don't want to
            // add overhead
            this->_store<m1>((uint64_t)new_ptr);
            return static_cast<packed_ptr<T, low_bits, high_bits>>(new_ptr);
        }
    }

    template<mem_order m = m_default>
    constexpr ALWAYS_INLINE packed_ptr<T, low_bits, high_bits>
                            set_ptr_known(T * const new_ptr, T * const guranteed_old_val) noexcept {

        if constexpr (low_bits || high_bits) {
            // the idea is we are creating a mask we can xor the value with so
            // negate the bits unique to old_val and set the bits unique to
            // new_ptr

            return static_cast<packed_ptr<T, low_bits, high_bits>>(
                this->ptr.fetch_xor(
                    ((uint64_t)new_ptr) ^ ((uint64_t)guranteed_old_val),
                    m));
        }
        else {
            this->_store<m>((uint64_t)new_ptr);
            return static_cast<packed_ptr<T, low_bits, high_bits>>(new_ptr);
            (void)guranteed_old_val;
        }
    }

    template<mem_order m = m_default>
    constexpr bool ALWAYS_INLINE
    ptr_equals(T * const other_ptr) const noexcept {
        return (this->get_ptr<m>() == other_ptr);
    }


    template<mem_order m = m_default>
    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE PURE_ATTR
    add_eq_ptr(const uint64_t plus_ptr) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>((sizeof(T) * plus_ptr)));
    }


    template<mem_order m = m_default>
    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    sub_eq_ptr(const uint64_t minus_ptr) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>((sizeof(T) * minus_ptr)));
    }

    template<mem_order m = m_default>
    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    incr_eq_ptr() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>((sizeof(T))));
    }

    template<mem_order m = m_default>
    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    decr_eq_ptr() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>((sizeof(T))));
    }

    template<mem_order m = m_default>
    constexpr ALWAYS_INLINE T *
                            add_ptr(const uint64_t plus_ptr) const noexcept {
        return this->get_ptr<m>() + plus_ptr;
    }

    template<mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR T *
                                      sub_ptr(const uint64_t sub_ptr) const noexcept {
        return this->get_ptr<m>() - sub_ptr;
    }


    //////////////////////////////////////////////////////////////////////
    // low bit helpers
    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        get_low_bits() const noexcept {
        return to_low_bits(this->_load<m>());
    }


    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        get_not_low_bits() const noexcept {
        return to_not_low_bits(this->_load<m>());
    }

    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        set_low_bits(const uint64_t new_low_bits) noexcept {
        uint64_t expected = this->_load<m1>();
        while (!(this->ptr.compare_exchange_weak(
            expected,
            new_low_bits | to_not_low_bits(expected),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }


    // this is a potentially safe and optimized version for setting low bits. It
    // is safe if high bits/ptr can be set concurrenctly to low bits being set
    // but no other operations to low bits take place during this operation
    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        set_low_bits_known(const uint64_t new_low_bits,
                           const uint64_t guranteed_old_val) noexcept {


        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_xor<m1>(new_low_bits ^ guranteed_old_val));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, bool>::type
    low_bits_equals(const uint64_t other_low_bits) const noexcept {
        return this->get_low_bits<m>() == other_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        add_eq_low_bits(const uint64_t plus_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>(plus_low_bits));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        sub_eq_low_bits(const uint64_t minus_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>(minus_low_bits));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        incr_eq_low_bits() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>(1));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        decr_eq_low_bits() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>(1));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        and_eq_low_bits(const uint64_t and_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_and<m>(and_low_bits | (~low_bits_mask())));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        or_eq_low_bits(const uint64_t or_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_or<m>(or_low_bits));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        xor_eq_low_bits(const uint64_t xor_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_xor<m>(xor_low_bits));
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        nand_eq_low_bits(const uint64_t nand_low_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_and<m>(~nand_low_bits));
    }

    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        shl_eq_low_bits(const uint64_t shl_low_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_low_bits(expected) | (to_low_bits(expected) << shl_low_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        shr_eq_low_bits(const uint64_t shr_low_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_low_bits(expected) | (to_low_bits(expected) >> shr_low_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        mult_eq_low_bits(const uint64_t mult_low_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_low_bits(expected) | (to_low_bits(expected) * mult_low_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _low_bits = low_bits,
             mem_order m1        = m_default,
             mem_order m2        = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_low_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        div_eq_low_bits(const uint64_t div_low_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_low_bits(expected) | (to_low_bits(expected) / div_low_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        add_low_bits(const uint64_t plus_low_bits) const noexcept {
        return this->get_low_bits<m>() + plus_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        sub_low_bits(const uint64_t minus_low_bits) const noexcept {
        return this->get_low_bits<m>() - minus_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    and_low_bits(const uint64_t and_low_bits) const noexcept {
        return this->_load<m>() & and_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        or_low_bits(const uint64_t or_low_bits) const noexcept {
        return this->get_low_bits<m>() | or_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    xor_low_bits(const uint64_t xor_low_bits) const noexcept {
        return this->get_low_bits<m>() ^ xor_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        nand_low_bits(const uint64_t nand_low_bits) const noexcept {
        return this->get_low_bits<m>() & (~nand_low_bits);
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE typename std::enable_if<_low_bits, uint64_t>::type
    shl_low_bits(const uint64_t shl_low_bits) const noexcept {
        return this->get_low_bits<m>() << shl_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        shr_low_bits(const uint64_t shr_low_bits) const noexcept {
        return this->get_low_bits<m>() >> shr_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        mult_low_bits(const uint64_t mult_low_bits) const noexcept {
        return this->get_low_bits<m>() * mult_low_bits;
    }

    template<uint32_t _low_bits = low_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_low_bits, uint64_t>::type
        div_low_bits(const uint64_t div_low_bits) const noexcept {
        return this->get_low_bits<m>() / div_low_bits;
    }


    //////////////////////////////////////////////////////////////////////
    // high bit helpers. For inplace versions (i.e if you want to explicitly
    // add (1 << high_bits_shift) directly instead of adding (1) and having
    // the add_high_bits function shift just use += operator
    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_high_bits() const noexcept {
        return to_high_bits(this->_load<m>());
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_not_high_bits() const noexcept {
        return to_not_high_bits(this->_load<m>());
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        get_high_bits_inplace() const noexcept {
        return to_high_bits_inplace(this->_load<m>());
    }


    template<uint32_t  _high_bits = high_bits,
             mem_order m1         = m_default,
             mem_order m2         = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        set_high_bits(const uint64_t new_high_bits) noexcept {
        uint64_t expected = this->_load<m1>();
        while (!(this->ptr.compare_exchange_weak(
            expected,
            (new_high_bits << high_bits_shift()) | to_not_high_bits(expected),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }


    // this is a potentially safe and optimized version for setting low bits. It
    // is safe if high bits/ptr can be set concurrenctly to low bits being set
    // but no other operations to low bits take place during this operation
    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        set_high_bits_known(const uint64_t new_high_bits,
                            const uint64_t guranteed_old_val) noexcept {


        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_xor((new_high_bits ^ guranteed_old_val)
                                    << high_bits_shift(),
                                m));
    }


    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE typename std::enable_if<_high_bits, bool>::type
    high_bits_equals(const uint64_t other_high_bits) const noexcept {
        return this->get_high_bits<m>() == other_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        add_eq_high_bits(const uint64_t plus_high_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>((plus_high_bits << high_bits_shift())));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        sub_eq_high_bits(const uint64_t minus_high_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>((minus_high_bits << high_bits_shift())));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        incr_eq_high_bits() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_add<m>((1UL) << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        decr_eq_high_bits() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_sub<m>((1UL) << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        and_eq_high_bits(const uint64_t and_high_bits) noexcept {
        return this->ptr.fetch_and<m>((and_high_bits << high_bits_shift()) |
                                      (~high_bits_mask()));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        or_eq_high_bits(const uint64_t or_high_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_or<m>(or_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        xor_eq_high_bits(const uint64_t xor_high_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_xor<m>(xor_high_bits << high_bits_shift()));
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        nand_eq_high_bits(const uint64_t nand_high_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_and<m>(~(nand_high_bits << high_bits_shift())));
    }

    template<uint32_t  _high_bits = high_bits,
             mem_order m1         = m_default,
             mem_order m2         = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        shl_eq_high_bits(const uint64_t shl_high_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_high_bits(expected) |
                (to_high_bits_inplace(expected) << shl_high_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _high_bits = high_bits,
             mem_order m1         = m_default,
             mem_order m2         = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        shr_eq_high_bits(const uint64_t shr_high_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_high_bits(expected) |
                (to_high_bits_inplace(expected) >> shr_high_bits),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _high_bits = high_bits,
             mem_order m1         = m_default,
             mem_order m2         = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        mult_eq_high_bits(const uint64_t mult_high_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_high_bits(expected) |
                ((to_high_bits(expected) * mult_high_bits)
                 << high_bits_shift()),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t  _high_bits = high_bits,
             mem_order m1         = m_default,
             mem_order m2         = m_default>
    constexpr ALWAYS_INLINE
        typename std::enable_if<_high_bits,
                                packed_ptr<T, low_bits, high_bits>>::type
        div_eq_high_bits(const uint64_t div_high_bits) noexcept {
        uint64_t expected = this->ptr;
        while (!(this->ptr.compare_exchange_weak(
            expected,
            to_not_high_bits(expected) |
                ((to_high_bits(expected) / div_high_bits) << high_bits_shift()),
            m1,
            m2)))
            ;
        return static_cast<packed_ptr<T, low_bits, high_bits>>(expected);
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        add_high_bits(const uint64_t plus_high_bits) const noexcept {
        return this->get_high_bits<m>() + plus_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        sub_high_bits(const uint64_t minus_high_bits) const noexcept {
        return this->get_high_bits<m>() - minus_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        and_high_bits(const uint64_t and_high_bits) const noexcept {
        return this->get_high_bits<m>() & and_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        or_high_bits(const uint64_t or_high_bits) const noexcept {
        return this->get_high_bits<m>() | or_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        xor_high_bits(const uint64_t xor_high_bits) const noexcept {
        return this->get_high_bits<m>() ^ xor_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        nand_high_bits(const uint64_t nand_high_bits) const noexcept {
        return this->get_high_bits<m>() & (~nand_high_bits);
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        shl_high_bits(const uint64_t shl_high_bits) const noexcept {
        return this->get_high_bits<m>() << shl_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        shr_high_bits(const uint64_t shr_high_bits) const noexcept {
        return this->get_high_bits<m>() >> shr_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        mult_high_bits(const uint64_t mult_high_bits) const noexcept {
        return this->get_high_bits<m>() * mult_high_bits;
    }

    template<uint32_t _high_bits = high_bits, mem_order m = m_default>
    constexpr ALWAYS_INLINE PURE_ATTR
        typename std::enable_if<_high_bits, uint64_t>::type
        div_high_bits(const uint64_t div_high_bits) const noexcept {
        return this->get_high_bits<m>() / div_high_bits;
    }


    //////////////////////////////////////////////////////////////////////
    // operator acting on entire ptr
    constexpr bool ALWAYS_INLINE
    operator==(const atomic_packed_ptr<T, low_bits, high_bits> & other) const
        noexcept {
        return this->_load() == other._load();
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(const packed_ptr<T, low_bits, high_bits> other) const noexcept {
        return this->_load() == other.ptr;
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(const uint64_t other) const noexcept {
        return this->_load() == other;
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const atomic_packed_ptr<T, low_bits, high_bits> other) const
        noexcept {
        return this->_load() != other._load();
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator!=(const uint64_t other) const noexcept {
        return this->_load() != other;
    }

    constexpr void ALWAYS_INLINE
    operator=(const packed_ptr<T, low_bits, high_bits> seed_other) noexcept {
        this->_store(seed_other._load());
    }

    constexpr void ALWAYS_INLINE
    operator=(const uint64_t seed_uint64_t) noexcept {
        this->ptr.store(seed_uint64_t);
    }

    constexpr void ALWAYS_INLINE
    operator=(T * const seed_ptr) noexcept {
        this->ptr.store((uint64_t)seed_ptr);
    }

    //////////////////////////////////////////////////////////////////////
    // operators acting on the pointer
    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator==(T * const other_ptr) const noexcept {
        return this->ptr_equals(other_ptr);
    }

    constexpr bool ALWAYS_INLINE PURE_ATTR
    operator!=(T * const other_ptr) const noexcept {
        return !this->ptr_equals(other_ptr);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator+=(const uint64_t plus_ptr) noexcept {
        static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->add_eq_ptr(plus_ptr) + plus_ptr * sizeof(T));
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator-=(const uint64_t minus_ptr) noexcept {
        static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->sub_eq_ptr(minus_ptr) - minus_ptr * sizeof(T));
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator++() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->incr_eq_ptr() + sizeof(T));
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator--() noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->decr_eq_ptr() + sizeof(T));
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator++(int) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->incr_eq_ptr());
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator--(int) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->decr_eq_ptr());
    }


    constexpr ALWAYS_INLINE PURE_ATTR T & operator*() noexcept {
        return *(this->get_ptr());
    }

    constexpr ALWAYS_INLINE PURE_ATTR const T & operator*() const noexcept {
        return *(this->get_ptr());
    }

    constexpr ALWAYS_INLINE PURE_ATTR T & operator[](
        const uint32_t i) noexcept {
        *(this->get_ptr() + i);
    }

    constexpr ALWAYS_INLINE PURE_ATTR const T & operator[](
        const uint32_t i) const noexcept {
        *(this->get_ptr() + i);
    }


    //////////////////////////////////////////////////////////////////////
    // operators acting on the bits
    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator&=(const uint64_t and_all_bits) noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_and(and_all_bits) & and_all_bits);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator|=(const uint64_t or_all_bits) noexcept {
        static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_or(or_all_bits) | or_all_bits);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator^=(const uint64_t xor_all_bits) noexcept {
        static_cast<packed_ptr<T, low_bits, high_bits>>(
            this->ptr.fetch_xor(xor_all_bits) ^ xor_all_bits);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator&(const uint64_t and_all_bits) const noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(this->_load() &
                                                               and_all_bits);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator|(const uint64_t or_all_bits) const noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(this->_load() |
                                                               or_all_bits);
    }

    constexpr packed_ptr<T, low_bits, high_bits> ALWAYS_INLINE
    operator^(const uint64_t xor_all_bits) const noexcept {
        return static_cast<packed_ptr<T, low_bits, high_bits>>(this->_load() ^
                                                               xor_all_bits);
    }
};

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE CONST_ATTR
operator==(T * const                                lhs,
           const packed_ptr<T, low_bits, high_bits> rhs) noexcept {
    return rhs.ptr_equals(lhs);
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE CONST_ATTR
operator==(const uint64_t                           lhs,
           const packed_ptr<T, low_bits, high_bits> rhs) noexcept {
    return rhs.ptr == lhs;
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE PURE_ATTR
operator==(T * const                                         lhs,
           const atomic_packed_ptr<T, low_bits, high_bits> & rhs) noexcept {
    return rhs.ptr_equals(lhs);
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE PURE_ATTR
operator==(const uint64_t                                    lhs,
           const atomic_packed_ptr<T, low_bits, high_bits> & rhs) noexcept {
    return rhs.ptr._load() == lhs;
}


template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE CONST_ATTR
operator!=(T * const                                lhs,
           const packed_ptr<T, low_bits, high_bits> rhs) noexcept {
    return !rhs.ptr_equals(lhs);
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE CONST_ATTR
operator!=(const uint64_t                           lhs,
           const packed_ptr<T, low_bits, high_bits> rhs) noexcept {
    return rhs.ptr != lhs;
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE PURE_ATTR
operator!=(T * const                                         lhs,
           const atomic_packed_ptr<T, low_bits, high_bits> & rhs) noexcept {
    return !rhs.ptr_equals(lhs);
}

template<typename T, uint32_t low_bits, uint32_t high_bits>
constexpr bool ALWAYS_INLINE PURE_ATTR
operator!=(const uint64_t                                    lhs,
           const atomic_packed_ptr<T, low_bits, high_bits> & rhs) noexcept {
    return rhs.ptr._load() != lhs;
}

#endif
