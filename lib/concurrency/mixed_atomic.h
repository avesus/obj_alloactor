#ifndef _MIXED_ATOMIC_H_
#define _MIXED_ATOMIC_H_
#include <stdint.h>
#include <util/atomic_utils.h>
#include <assert.h>

namespace matm {

using mem_order = int32_t;

template<typename T,
         atomics::safety s         = atomics::safe,
         mem_order       m_default = __ATOMIC_RELAXED>
struct c_atomic {

    static constexpr const bool weak   = true;
    static constexpr const bool strong = false;
    using internal_T                   = atomics::atomic_t<T, s>;
    internal_T v;

    c_atomic() noexcept  = default;
    ~c_atomic() noexcept = default;

    c_atomic(const T _v) noexcept : v(_v) {}


    constexpr bool ALWAYS_INLINE
    is_lock_free() noexcept {
        return __atomic_is_lock_free(sizeof(T), &v);
    }

    constexpr bool ALWAYS_INLINE
    is_always_lock_free() noexcept {
        return __atomic_is_always_lock_free(sizeof(T), &v);
    }

    constexpr void ALWAYS_INLINE
    store(const T _v, mem_order m) noexcept {
        __atomic_store_n(&v, _v, m);
    }

    constexpr void ALWAYS_INLINE
    store(const T _v) noexcept {
        // on x86 with __ATOMIC_RELAXED (no memory barrier) store is just mov
        if constexpr (m_default == __ATOMIC_RELAXED) {
            this->v = _v;
        }
        else {
            return this->store(_v, m_default);
        }
    }

    constexpr T ALWAYS_INLINE
    load(mem_order m) const noexcept {
        return __atomic_load_n(&v, m);
    }

    constexpr T ALWAYS_INLINE
    load() const noexcept {
        // on x86 with __ATOMIC_RELAXED (no memory barrier) load is just mov
        if constexpr (m_default == __ATOMIC_RELAXED) {
            return v;
        }
        else {
            return __atomic_load_n(&v, m_default);
        }
    }


    constexpr T ALWAYS_INLINE
    exchange(const T _v, mem_order m = m_default) noexcept {
        return __atomic_exchange(&v, _v, m);
    }

    constexpr bool ALWAYS_INLINE
    compare_exchange_weak(T &       expected,
                          T         _v,
                          mem_order m1 = m_default,
                          mem_order m2 = m_default) noexcept {
        return __atomic_compare_exchange_n(&v, &expected, _v, weak, m1, m2);
    }

    constexpr bool ALWAYS_INLINE
    compare_exchange_strong(T &       expected,
                            T         _v,
                            mem_order m1 = m_default,
                            mem_order m2 = m_default) noexcept {
        return __atomic_compare_exchange_n(&v, &expected, _v, strong, m1, m2);
    }

    constexpr T ALWAYS_INLINE
    fetch_add(const T _v, mem_order m = m_default) noexcept {
        return __atomic_fetch_add(&v, _v, m);
    }
    constexpr T ALWAYS_INLINE
    fetch_sub(const T _v, mem_order m = m_default) noexcept {
        return __atomic_fetch_sub(&v, _v, m);
    }
    constexpr T ALWAYS_INLINE
    fetch_and(const T _v, mem_order m = m_default) noexcept {
        return __atomic_fetch_and(&v, _v, m);
    }
    constexpr T ALWAYS_INLINE
    fetch_or(const T _v, mem_order m = m_default) noexcept {
        return __atomic_fetch_or(&v, _v, m);
    }
    constexpr T ALWAYS_INLINE
    fetch_xor(const T _v, mem_order m = m_default) noexcept {
        return __atomic_fetch_xor(&v, _v, m);
    }

    constexpr ALWAYS_INLINE operator T() const noexcept {
        return this->load();
    }

    constexpr T ALWAYS_INLINE
    operator=(const T _v) noexcept {
        __atomic_store(&v, _v);
    }

    constexpr T ALWAYS_INLINE
    operator=(c_atomic<T> _v) noexcept {
        __atomic_store(&v, _v.load());
    }

    constexpr T ALWAYS_INLINE
    operator++() noexcept {
        return __atomic_fetch_add(&v, 1, m_default) + 1;
    }

    constexpr T ALWAYS_INLINE
    operator--() noexcept {
        return __atomic_fetch_sub(&v, 1, m_default) - 1;
    }

    constexpr T ALWAYS_INLINE
    operator++(int) noexcept {
        return __atomic_fetch_add(&v, 1, m_default);
    }

    constexpr T ALWAYS_INLINE
    operator--(int) noexcept {
        return __atomic_fetch_sub(&v, 1, m_default);
    }

    constexpr T ALWAYS_INLINE
    operator+=(const T _v) noexcept {
        return __atomic_fetch_add(&v, _v, m_default) + v;
    }
    constexpr T ALWAYS_INLINE
    operator-=(const T _v) noexcept {
        return __atomic_fetch_sub(&v, _v, m_default) - v;
    }
    constexpr T ALWAYS_INLINE
    operator&=(const T _v) noexcept {
        return __atomic_fetch_and(&v, _v, m_default) & v;
    }
    constexpr T ALWAYS_INLINE
    operator|=(const T _v) noexcept {
        return __atomic_fetch_or(&v, _v, m_default) | v;
    }
    constexpr T ALWAYS_INLINE
    operator^=(const T _v) noexcept {
        return __atomic_fetch_add(&v, _v, m_default) ^ v;
    }
};
}  // namespace matm

#endif
