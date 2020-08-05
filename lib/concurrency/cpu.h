#ifndef _CPU_SET_H_
#define _CPU_SET_H_

#include <sched.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>
#include <type_traits>

#include <optimized/bits.h>
#include <optimized/const_math.h>
#include <optimized/packed_ptr.h>

#include <util/const_utils.h>


namespace cpu {

//////////////////////////////////////////////////////////////////////
// this is basically a faster version of get_cpu (spinning around .8 ns)
const uint32_t PERCPU_RSEQ_SIGNATURE = 0x53053053;
const uint32_t __NR_rseq             = 334;

struct kernel_rseq {
    unsigned           cpu_id_start;
    unsigned           cpu_id;
    unsigned long long rseq_cs;
    unsigned           flags;
} __attribute__((aligned(4 * sizeof(unsigned long long))));

static __thread volatile struct kernel_rseq __rseq_abi;
static __thread uint32_t                    __rseq_refcount;

// init is required per thread
uint32_t
init_get_cur_cpu() noexcept {
    if (BRANCH_LIKELY(++__rseq_refcount > 1)) {
        return 1;
    }
    const uint32_t ret = syscall(__NR_rseq,
                                 &__rseq_abi,
                                 sizeof(__rseq_abi),
                                 0,
                                 PERCPU_RSEQ_SIGNATURE);
    if (ret == 0) {
        return 1;
    }
    else {
        // basically means try again later
        --__rseq_refcount;
        return 0;
    }
}

// just return cpu (use only if safe i.e init has already been called)
uint32_t ALWAYS_INLINE PURE_ATTR
get_cur_cpu() noexcept {
    return __rseq_abi.cpu_id;
}

// just return cpu (use if unknown if init has been called)
uint32_t ALWAYS_INLINE
get_cur_cpu_init() noexcept {
    init_get_cur_cpu();
    return __rseq_abi.cpu_id;
}


template<uint32_t NCPU>
struct inner_iterator;

template<uint32_t NCPU>
struct outer_iterator;

template<uint32_t NCPU>
struct destructive_inner_iterator;

template<uint32_t NCPU>
struct destructive_outer_iterator;


template<uint32_t NCPU>
struct forward_iterator;

template<uint32_t NCPU>
struct backward_iterator;

// todo: support AVX
template<uint32_t NCPU>
struct var_cpu_set {

    uint64_t
        cpu_vecs[cmath::roundup_div<uint32_t>(NCPU,
                                              cutil::sizeof_bits<uint64_t>())];

    template<uint32_t N = NCPU>
    using _cpu_set_iterator =
        typename std::conditional<(N > cutil::sizeof_bits<uint64_t>()),
                                  outer_iterator<N>,
                                  inner_iterator<N>>::type;


    template<uint32_t N = NCPU>
    using _d_cpu_set_iterator =
        typename std::conditional<(N > cutil::sizeof_bits<uint64_t>()),
                                  destructive_outer_iterator<N>,
                                  destructive_inner_iterator<N>>::type;

    template<uint32_t N = NCPU>
    using _f_cpu_set_iterator =
        typename std::conditional<(N > cutil::sizeof_bits<uint64_t>()),
                                  forward_iterator<N>,
                                  inner_iterator<N>>::type;

    template<uint32_t N = NCPU>
    using _b_cpu_set_iterator =
        typename std::conditional<(N > cutil::sizeof_bits<uint64_t>()),
                                  backward_iterator<N>,
                                  inner_iterator<N>>::type;


    using cpu_set_iterator   = _cpu_set_iterator<NCPU>;
    using d_cpu_set_iterator = _d_cpu_set_iterator<NCPU>;
    using f_cpu_set_iterator = _f_cpu_set_iterator<NCPU>;
    using b_cpu_set_iterator = _b_cpu_set_iterator<NCPU>;


    var_cpu_set() noexcept  = default;
    ~var_cpu_set() noexcept = default;


    var_cpu_set(const uint32_t cpu) noexcept {
        this->cpu_zero();
        this->cpu_set(cpu);
    }

    var_cpu_set(const var_cpu_set<NCPU> & other) noexcept {
        this->cpu_vecs[0] = other.cpu_vecs[0];
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] = other.cpu_vecs[i];
        }
    }

    constexpr uint32_t ALWAYS_INLINE
    n_set_vecs() const noexcept {
        return cmath::roundup_div<uint32_t>(NCPU,
                                            cutil::sizeof_bits<uint64_t>());
    }

    constexpr uint32_t ALWAYS_INLINE
    get_size() const noexcept {
        return sizeof(var_cpu_set<NCPU>);
    }

    constexpr  ALWAYS_INLINE cpu_set_t *
    get_cpu_set_t() const noexcept {
        return (cpu_set_t *)(&(this->cpu_vecs[0]));
    }


    constexpr void ALWAYS_INLINE
    cpu_zero() noexcept {
        this->cpu_vecs[0] = 0;
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] = 0;
        }
    }

    template<uint32_t N = NCPU>
    constexpr ALWAYS_INLINE
        typename std::enable_if<!(N % cutil::sizeof_bits<uint64_t>()),
                                void>::type
        cpu_set_all() noexcept {
        for (uint32_t i = 0; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] = (~(0UL));
        }
    }

    template<uint32_t N = NCPU>
    constexpr ALWAYS_INLINE
        typename std::enable_if<(N % cutil::sizeof_bits<uint64_t>()),
                                void>::type
        cpu_set_all() noexcept {
        for (uint32_t i = 0; i < (this->n_set_vecs() - 1); ++i) {
            this->cpu_vecs[i] = (~(0UL));
        }
        this->cpu_vecs[this->n_set_vecs() - 1] =
            (((1UL) << (NCPU % cutil::sizeof_bits<uint64_t>())) - 1);
    }

    constexpr void ALWAYS_INLINE
    cpu_set(const uint32_t cpu) noexcept {
        this->cpu_vecs[cpu / 64] |= ((1UL) << (cpu % 64));
    }

    constexpr void ALWAYS_INLINE
    cpu_clr(const uint32_t cpu) noexcept {
        this->cpu_vecs[cpu / 64] &= (~((1UL) << (cpu % 64)));
    }

    constexpr void ALWAYS_INLINE
    cpu_isset_clr(const uint32_t cpu) noexcept {
        this->cpu_vecs[cpu / 64] ^= ((1UL) << (cpu % 64));
    }

    constexpr uint32_t ALWAYS_INLINE
    cpu_isset(const uint32_t cpu) const noexcept {
        return (this->cpu_vecs[cpu / 64] >> (cpu % 64)) & 0x1;
    }

    constexpr uint32_t ALWAYS_INLINE
    cpu_count() const noexcept {
        // Possibly replace with precompiled foreach
        uint32_t sum = bits::bitcount<uint64_t>(this->cpu_vecs[0]);
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            sum += bits::bitcount<uint64_t>(this->cpu_vecs[i]);
        }
        return sum;
    }

    constexpr void ALWAYS_INLINE
    operator&=(const var_cpu_set<NCPU> & other) noexcept {
        this->cpu_vecs[0] &= other.cpu_vecs[0];
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] &= other.cpu_vecs[i];
        }
    }

    constexpr void ALWAYS_INLINE
    operator^=(const var_cpu_set<NCPU> & other) noexcept {
        this->cpu_vecs[0] ^= other.cpu_vecs[0];
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] ^= other.cpu_vecs[i];
        }
    }
    constexpr void ALWAYS_INLINE
    operator|=(const var_cpu_set<NCPU> & other) noexcept {
        this->cpu_vecs[0] |= other.cpu_vecs[0];
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] |= other.cpu_vecs[i];
        }
    }

    constexpr bool ALWAYS_INLINE
    operator==(const var_cpu_set<NCPU> & other) const noexcept {
        bool res = (this->cpu_vecs[0] == other.cpu_vecs[0]);
        for (uint32_t i = 1; res && i < this->n_set_vecs(); ++i) {
            res = (this->cpu_vecs[i] == other.cpu_vecs[i]);
        }
        return res;
    }

    constexpr void ALWAYS_INLINE
    operator=(const var_cpu_set<NCPU> & other) noexcept {
        this->cpu_vecs[0] = other.cpu_vecs[0];
        for (uint32_t i = 1; i < this->n_set_vecs(); ++i) {
            this->cpu_vecs[i] = other.cpu_vecs[i];
        }
    }

    template<uint32_t _NCPU>
    constexpr ALWAYS_INLINE
        typename std::enable_if<(_NCPU <= cutil::sizeof_bits<uint64_t>()),
                                void>::type
        operator=(const uint64_t other) noexcept {
        this->cpu_vecs[0] = other;
    }

    constexpr uint32_t ALWAYS_INLINE
    first() const noexcept {
        uint32_t i;
        for (i = 0; i < this->n_set_vecs() && (!(this->cpu_vecs[i])); ++i) {
        }
        return (i == this->n_set_vecs())
                   ? NCPU
                   : (cutil::sizeof_bits<uint64_t>() * i +
                      bits::find_first_one<uint64_t>(this->cpu_vecs[i]));
    }

    constexpr uint32_t ALWAYS_INLINE
    last() const noexcept {
        int32_t i;
        for (i = this->n_set_vecs() - 1; i >= 0 && (!(this->cpu_vecs[i]));
             --i) {
        }
        return (i == (-1)) ? NCPU
                           : (cutil::sizeof_bits<uint64_t>() * i +
                              bits::find_last_one<uint64_t>(this->cpu_vecs[i]));
    }

    void
    to_string() noexcept {
        for (uint32_t i = 0; i < this->n_set_vecs(); ++i) {
            fprintf(stderr, "%d: [%016lX]\n", i, this->cpu_vecs[i]);
        }
    }

    constexpr cpu_set_iterator ALWAYS_INLINE
    begin() const noexcept {
        return cpu_set_iterator(this);
    }
    constexpr cpu_set_iterator ALWAYS_INLINE
    end() const noexcept {
        return cpu_set_iterator();
    }

    constexpr d_cpu_set_iterator ALWAYS_INLINE
    dbegin() const noexcept {
        return d_cpu_set_iterator(this);
    }
    constexpr d_cpu_set_iterator ALWAYS_INLINE
    dend() const noexcept {
        return d_cpu_set_iterator();
    }

    constexpr f_cpu_set_iterator ALWAYS_INLINE
    fbegin() const noexcept {
        return f_cpu_set_iterator(this);
    }
    constexpr f_cpu_set_iterator ALWAYS_INLINE
    fend() const noexcept {
        return f_cpu_set_iterator();
    }

    constexpr b_cpu_set_iterator ALWAYS_INLINE
    bbegin() const noexcept {
        return b_cpu_set_iterator(this);
    }
    constexpr b_cpu_set_iterator ALWAYS_INLINE
    bend() const noexcept {
        return b_cpu_set_iterator();
    }
};


template<uint32_t NCPU>
struct forward_iterator {
    inner_iterator<NCPU>        it;
    packed_ptr<uint64_t, 0, 16> cpu_vecs;

    ~forward_iterator() noexcept = default;

    forward_iterator(const forward_iterator<NCPU> & other) noexcept {
        this->it       = other.it;
        this->cpu_vecs = other.cpu_vecs;
    }

    forward_iterator(const var_cpu_set<NCPU> & _cpu_set) noexcept
        : forward_iterator((uint64_t * const)(&(_cpu_set.cpu_vecs[0]))) {}

    forward_iterator(const uint64_t * const _cpu_vecs) noexcept {
        int64_t lo_idx = this->n_set_vecs();
        this->it       = 0;
        for (uint64_t i = 0; i < this->n_set_vecs(); ++i) {
            if (_cpu_vecs[i]) {
                lo_idx   = i;
                this->it = _cpu_vecs[i];
                break;
            }
        }

        this->cpu_vecs = ((uint64_t)_cpu_vecs) | (this->init_indexes(lo_idx));
    }

    constexpr forward_iterator() noexcept {
        this->cpu_vecs = (this->init_indexes(this->n_set_vecs()));
    }

    constexpr uint64_t ALWAYS_INLINE
    n_set_vecs() const noexcept {
        return cmath::roundup_div<uint64_t>(NCPU,
                                            cutil::sizeof_bits<uint64_t>());
    }

    constexpr uint64_t ALWAYS_INLINE
    init_indexes(const int64_t lo_idx) noexcept {
        return lo_idx << this->cpu_vecs.high_bits_shift();
    }

    constexpr uint64_t ALWAYS_INLINE
    incr_lo_idx() noexcept {
        return this->cpu_vecs.incr_eq_high_bits();
    }


    constexpr uint64_t ALWAYS_INLINE
    get_lo_idx() const noexcept {
        return this->cpu_vecs.get_high_bits();
    }

    constexpr uint32_t ALWAYS_INLINE
    operator++() noexcept {
        const uint32_t ret = ++(this->it) + (cutil::sizeof_bits<uint64_t>() *
                                             this->get_lo_idx());

        while ((this->get_lo_idx() < this->n_set_vecs()) && (!(this->it))) {
            // it might be faster to have a seperate conditional
            this->it =
                this->cpu_vecs[this->incr_lo_idx() & (this->n_set_vecs() - 1)];
        }
        return ret;
    }


    // generally dont use this...
    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator++(int) noexcept {
        return var_cpu_set<NCPU>((++this[0]));
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const forward_iterator<NCPU> & other) const noexcept {
        return !(this[0] == other);
    }


    constexpr bool ALWAYS_INLINE
    operator==(const forward_iterator<NCPU> & other) const noexcept {
        if (this->get_lo_idx() == this->n_set_vecs()) {
            return other.get_lo_idx() == this->n_set_vecs();
        }
        else {
            bool res = (this->get_lo_idx() == other.get_lo_idx()) &&
                       (this->it == other.it);
            for (uint64_t i = this->get_lo_idx() + 1;
                 res && i <= this->n_set_vecs();
                 ++i) {
                res = (this->cpu_vecs[i] == other.cpu_vecs[i]);
            }
            return res;
        }
    }
};


template<uint32_t NCPU>
struct backward_iterator {
    inner_iterator<NCPU>            it;
    packed_ptr<uint64_t, 0, 16>     cpu_vecs;
    static const constexpr uint32_t hi_idx_end = (1 << 16) - 1;

    ~backward_iterator() noexcept = default;

    backward_iterator(const backward_iterator<NCPU> & other) noexcept {
        this->it       = other.it;
        this->cpu_vecs = other.cpu_vecs;
    }

    backward_iterator(const var_cpu_set<NCPU> & _cpu_set) noexcept
        : backward_iterator((uint64_t * const)(&(_cpu_set.cpu_vecs[0]))) {}

    backward_iterator(const uint64_t * const _cpu_vecs) noexcept {
        int64_t hi_idx = this->hi_idx_end;
        this->it       = 0;
        for (int64_t i = this->n_set_vecs() - 1; i >= 0; --i) {
            if (_cpu_vecs[i]) {
                hi_idx   = i;
                this->it = _cpu_vecs[i];
                break;
            }
        }
        this->cpu_vecs = ((uint64_t)_cpu_vecs) | this->init_indexes(hi_idx);
    }

    constexpr backward_iterator() noexcept {
        this->cpu_vecs = this->init_indexes(this->hi_idx_end);
    }

    constexpr uint32_t ALWAYS_INLINE
    n_set_vecs() const noexcept {
        return cmath::roundup_div<uint32_t>(NCPU,
                                            cutil::sizeof_bits<uint64_t>());
    }

    constexpr uint64_t ALWAYS_INLINE
    init_indexes(const int64_t hi_idx) noexcept {
        return hi_idx << this->cpu_vecs.high_bits_shift();
    }

    constexpr int32_t ALWAYS_INLINE
    decr_hi_idx() noexcept {
        return this->cpu_vecs.decr_eq_high_bits();
    }

    constexpr int32_t ALWAYS_INLINE
    get_hi_idx() const noexcept {
        return this->cpu_vecs.get_high_bits();
    }


    constexpr uint32_t ALWAYS_INLINE
    operator--() noexcept {
        const uint32_t ret = --(this->it) + (cutil::sizeof_bits<uint64_t>() *
                                             this->get_hi_idx());

        while ((this->get_hi_idx() != this->hi_idx_end) && (!(this->it))) {
            this->it =
                this->cpu_vecs[this->decr_hi_idx() & (this->n_set_vecs() - 1)];
        }
        return ret;
    }


    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator--(int) noexcept {
        return var_cpu_set<NCPU>((--this[0]));
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const backward_iterator<NCPU> & other) const noexcept {
        return !(this[0] == other);
    }


    constexpr bool ALWAYS_INLINE
    operator==(const backward_iterator<NCPU> & other) const noexcept {
        if (this->get_hi_idx() == this->hi_idx_end) {
            return other.get_hi_idx() == hi_idx_end;
        }
        else {
            bool res = (this->get_hi_idx() == other.get_hi_idx()) &&
                       (this->it == other.it);
            for (int32_t i = 0; res && i <= (this->get_hi_idx() - 1); ++i) {
                res = (this->cpu_vecs[i] == other.cpu_vecs[i]);
            }
            return res;
        }
    }
};


template<uint32_t NCPU>
struct outer_iterator {
    inner_iterator<NCPU>        lo_it;
    inner_iterator<NCPU>        hi_it;
    packed_ptr<uint64_t, 2, 16> cpu_vecs;

    ~outer_iterator() noexcept = default;

    outer_iterator(const outer_iterator<NCPU> & other) noexcept {
        this->lo_it    = other.lo_it;
        this->hi_it    = other.hi_it;
        this->cpu_vecs = other.cpu_vecs;
    }

    outer_iterator(const var_cpu_set<NCPU> & _cpu_set) noexcept
        : outer_iterator((uint64_t * const)(&(_cpu_set.cpu_vecs[0]))) {}

    outer_iterator(const uint64_t * const _cpu_vecs) noexcept {
        int64_t lo_idx = this->n_set_vecs();
        int64_t hi_idx = 0;
        for (uint64_t i = 0; i < this->n_set_vecs(); ++i) {
            if (_cpu_vecs[i]) {
                lo_idx      = i;
                this->lo_it = _cpu_vecs[i];
                break;
            }
        }
        for (int64_t i = this->n_set_vecs() - 1; i >= lo_idx; --i) {
            if (_cpu_vecs[i]) {
                hi_idx      = i;
                this->hi_it = _cpu_vecs[i];
                break;
            }
        }
        this->cpu_vecs = ((uint64_t)_cpu_vecs) |
                         (this->init_indexes(lo_idx, hi_idx)) |
                         (hi_idx == lo_idx);
    }

    constexpr outer_iterator() noexcept {
        this->cpu_vecs = (this->init_indexes(this->n_set_vecs(), 0));
    }

    constexpr uint32_t ALWAYS_INLINE
    n_set_vecs() const noexcept {
        return cmath::roundup_div<uint32_t>(NCPU,
                                            cutil::sizeof_bits<uint64_t>());
    }

    constexpr uint64_t ALWAYS_INLINE
    init_indexes(const int64_t lo_idx, const int64_t hi_idx) noexcept {
        return ((((int64_t)(this->n_set_vecs() - 1) - hi_idx) << 8) | lo_idx)
               << (this->cpu_vecs.high_bits_shift());
    }

    constexpr uint32_t ALWAYS_INLINE
    decr_hi_idx() noexcept {
        return (this->n_set_vecs() - 1) -
               (this->cpu_vecs.add_eq_high_bits((1 << 8)) >> 8);
    }

    constexpr uint64_t ALWAYS_INLINE
    incr_lo_idx() noexcept {
        return this->cpu_vecs.incr_eq_high_bits() & 0xff;
    }

    constexpr int32_t ALWAYS_INLINE
    get_hi_idx() const noexcept {
        return (this->n_set_vecs() - 1) - this->cpu_vecs.shr_high_bits(8);
    }

    constexpr int32_t ALWAYS_INLINE
    get_lo_idx() const noexcept {
        return this->cpu_vecs.and_high_bits(0xff);
    }

    constexpr uint64_t ALWAYS_INLINE
    is_lo_redirect() const noexcept {
        return this->cpu_vecs.and_low_bits(0x1);
    }

    constexpr uint64_t ALWAYS_INLINE
    is_hi_redirect() const noexcept {
        return this->cpu_vecs.and_low_bits(0x2);
    }

    constexpr void ALWAYS_INLINE
    set_lo_redirect(const uint32_t b) noexcept {
        this->cpu_vecs.or_eq_low_bits(b);
    }

    constexpr void ALWAYS_INLINE
    set_hi_redirect(const uint32_t b) noexcept {
        this->cpu_vecs.or_eq_low_bits(b << 1);
    }

    constexpr inner_iterator<NCPU> ALWAYS_INLINE
    get_lo_it_copy() const noexcept {
        return this->is_lo_redirect() ? this->hi_it : this->lo_it;
    }

    constexpr inner_iterator<NCPU> ALWAYS_INLINE
    get_hi_it_copy() const noexcept {
        return this->is_hi_redirect() ? this->lo_it : this->hi_it;
    }


    constexpr uint32_t ALWAYS_INLINE
    operator++() noexcept {
        const uint32_t ret =
            (++(this->is_lo_redirect() ? this->hi_it : this->lo_it)) +
            (cutil::sizeof_bits<uint64_t>() * this->get_lo_idx());
        while ((this->get_lo_idx() <= this->get_hi_idx()) &&
               (!(this->get_lo_it_copy()))) {
            // it might be faster to have a seperate conditional
            this->lo_it =
                this->cpu_vecs[this->incr_lo_idx() & (this->n_set_vecs() - 1)];
            this->set_lo_redirect(this->get_lo_idx() == this->get_hi_idx());
        }
        return ret;
    }

    constexpr uint32_t ALWAYS_INLINE
    operator--() noexcept {
        const uint32_t ret =
            (--(this->is_hi_redirect() ? this->lo_it : this->hi_it)) +
            (cutil::sizeof_bits<uint64_t>() * this->get_hi_idx());
        while (((this->get_lo_idx() <= this->get_hi_idx())) &&
               (!(this->get_hi_it_copy()))) {
            // it might be faster to have a seperate conditional
            this->hi_it =
                this->cpu_vecs[this->decr_hi_idx() & (this->n_set_vecs() - 1)];
            this->set_hi_redirect(this->get_lo_idx() == this->get_hi_idx());
        }
        return ret;
    }

    // generally dont use this...
    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator++(int) noexcept {
        return var_cpu_set<NCPU>((++this[0]));
    }

    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator--(int) noexcept {
        return var_cpu_set<NCPU>((--this[0]));
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const outer_iterator<NCPU> & other) const noexcept {
        return !(this[0] == other);
    }


    constexpr bool ALWAYS_INLINE
    operator==(const outer_iterator<NCPU> & other) const noexcept {
        if (this->get_lo_idx() > this->get_hi_idx()) {
            return other.get_lo_idx() > other.get_hi_idx();
        }
        else {
            bool res = (this->get_lo_idx() == other.get_lo_idx()) &&
                       (this->get_hi_idx() == other.get_hi_idx()) &&
                       (this->get_lo_it_copy() == other.get_lo_it_copy()) &&
                       (this->get_hi_it_copy() == other.get_hi_it_copy());
            for (int32_t i = this->get_lo_idx() + 1;
                 res && i <= (this->get_hi_idx() - 1);
                 ++i) {
                res = (this->cpu_vecs[i] == other.cpu_vecs[i]);
            }
            return res;
        }
    }
};

template<uint32_t NCPU>
struct inner_iterator {
    uint64_t cpu_vec;

    inner_iterator() noexcept  = default;
    ~inner_iterator() noexcept = default;

    inner_iterator(const uint64_t _cpu_vec) : cpu_vec(_cpu_vec) {}

    inner_iterator(const var_cpu_set<NCPU> _cpu_set) noexcept
        : cpu_vec(_cpu_set.cpu_vecs[0]) {}

    constexpr uint32_t ALWAYS_INLINE
    operator++() noexcept {
        uint32_t next_cpu = bits::find_first_one<uint64_t>(this->cpu_vec);
        this->cpu_vec ^= ((1UL) << next_cpu);
        return next_cpu;
    }

    constexpr uint32_t ALWAYS_INLINE
    operator--() noexcept {
        uint32_t next_cpu = bits::find_last_one<uint64_t>(this->cpu_vec);
        this->cpu_vec ^= ((1UL) << next_cpu);
        return next_cpu;
    }

    constexpr uint64_t ALWAYS_INLINE
    operator++(int) noexcept {

        uint32_t next_cpu = bits::find_first_one<uint64_t>(this->cpu_vec);
        this->cpu_vec ^= ((1UL) << next_cpu);
        return (1UL) << next_cpu;
    }

    constexpr uint64_t ALWAYS_INLINE
    operator--(int) noexcept {
        uint32_t next_cpu = bits::find_last_one<uint64_t>(this->cpu_vec);
        this->cpu_vec ^= ((1UL) << next_cpu);
        return (1UL) << next_cpu;
    }

    constexpr bool ALWAYS_INLINE
    operator==(const inner_iterator other) const noexcept {
        return this->cpu_vec == other.cpu_vec;
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const inner_iterator other) const noexcept {
        return this->cpu_vec != other.cpu_vec;
    }

    constexpr void ALWAYS_INLINE
    operator=(const inner_iterator other) noexcept {
        this->cpu_vec = other.cpu_vec;
    }

    constexpr void ALWAYS_INLINE
    operator=(const uint64_t _cpu_vec) noexcept {
        this->cpu_vec = _cpu_vec;
    }

    constexpr bool ALWAYS_INLINE operator!() const noexcept {
        return !(this->cpu_vec);
    }
};


template<uint32_t NCPU>
struct destructive_outer_iterator {
    packed_ptr<uint64_t, 0, 16> cpu_vecs;

    ~destructive_outer_iterator() noexcept = default;

    destructive_outer_iterator(
        const destructive_outer_iterator<NCPU> & other) noexcept
        : cpu_vecs(other.cpu_vecs) {}

    destructive_outer_iterator(const var_cpu_set<NCPU> & _cpu_set) noexcept
        : destructive_outer_iterator(
              (uint64_t * const)(&(_cpu_set.cpu_vecs[0]))) {}

    destructive_outer_iterator(uint64_t * const _cpu_vecs) noexcept {
        int64_t lo_idx = this->n_set_vecs();
        int64_t hi_idx = 0;
        for (uint64_t i = 0; i < this->n_set_vecs(); ++i) {
            if (_cpu_vecs[i]) {
                lo_idx = i;
                break;
            }
        }
        for (int64_t i = this->n_set_vecs() - 1; i >= lo_idx; --i) {
            if (_cpu_vecs[i]) {
                hi_idx = i;
                break;
            }
        }

        this->cpu_vecs =
            ((uint64_t)_cpu_vecs) | (this->init_indexes(lo_idx, hi_idx));
    }

    constexpr destructive_outer_iterator() noexcept {
        this->cpu_vecs = (this->init_indexes(this->n_set_vecs(), 0));
    }


    constexpr uint32_t ALWAYS_INLINE
    n_set_vecs() const noexcept {
        return cmath::roundup_div<uint32_t>(NCPU,
                                            cutil::sizeof_bits<uint64_t>());
    }

    constexpr uint64_t ALWAYS_INLINE
    init_indexes(const int64_t lo_idx, const int64_t hi_idx) noexcept {
        return ((((int64_t)this->n_set_vecs() - hi_idx) << 8) | lo_idx)
               << (this->cpu_vecs.high_bits_shift());
    }

    constexpr void ALWAYS_INLINE
    decr_hi_idx() noexcept {
        this->cpu_vecs.add_eq_high_bits((1 << 8));
    }

    constexpr void ALWAYS_INLINE
    incr_lo_idx() noexcept {
        this->cpu_vecs.incr_eq_high_bits();
    }

    constexpr int32_t ALWAYS_INLINE
    get_hi_idx() const noexcept {
        return this->n_set_vecs() - this->cpu_vecs.shr_high_bits(8);
    }

    constexpr int32_t ALWAYS_INLINE
    get_lo_idx() const noexcept {
        return this->cpu_vecs.and_high_bits(0xff);
    }

    constexpr uint32_t ALWAYS_INLINE
    get_first_p1() const noexcept {
        return bits::find_first_one<uint64_t>(
                   this->cpu_vecs[this->get_lo_idx()]) +
               (cutil::sizeof_bits<uint64_t>() * this->get_lo_idx());
    }

    constexpr uint32_t ALWAYS_INLINE
    get_first() noexcept {
        const uint32_t ret = this->get_first_p1();
        this->cpu_vecs[this->get_lo_idx()] ^=
            ((1UL) << (ret % cutil::sizeof_bits<uint64_t>()));

        while ((this->get_lo_idx() <= this->get_hi_idx()) &&
               (this->cpu_vecs[this->get_lo_idx()] == 0)) {
            this->incr_lo_idx();
        }
        return ret;
    }

    constexpr uint32_t ALWAYS_INLINE
    get_last_p1() const noexcept {
        return bits::find_last_one<uint64_t>(
                   this->cpu_vecs[this->get_hi_idx()]) +
               (cutil::sizeof_bits<uint64_t>() * this->get_hi_idx());
    }


    constexpr uint32_t ALWAYS_INLINE
    get_last() noexcept {
        const uint32_t ret = this->get_last_p1();
        this->cpu_vecs[this->get_hi_idx()] ^=
            ((1UL) << (ret % cutil::sizeof_bits<uint64_t>()));

        while ((this->get_lo_idx() <= this->get_hi_idx()) &&
               (this->cpu_vecs[this->get_hi_idx()] == 0)) {
            this->decr_hi_idx();
        }
        return ret;
    }

    constexpr uint32_t ALWAYS_INLINE
    operator++() noexcept {
        return this->get_first();
    }

    constexpr uint32_t ALWAYS_INLINE
    operator--() noexcept {
        return this->get_last();
    }

    // generally dont use this...
    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator++(int) noexcept {
        return var_cpu_set<NCPU>(this->get_first());
    }

    constexpr var_cpu_set<NCPU> ALWAYS_INLINE
    operator--(int) noexcept {
        return var_cpu_set<NCPU>(this->get_last());
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const destructive_outer_iterator<NCPU> & other) const noexcept {
        return !(this[0] == other);
    }


    constexpr bool ALWAYS_INLINE
    operator==(const destructive_outer_iterator<NCPU> & other) const noexcept {
        if (this->cpu_vecs == other.cpu_vecs) {
            return true;
        }
        else if (this->get_lo_idx() > this->get_hi_idx()) {
            return other.get_lo_idx() > other.get_hi_idx();
        }
        else {
            bool res = (this->get_lo_idx() == other.get_lo_idx()) &&
                       (this->get_hi_idx() == other.get_hi_idx());
            for (int32_t i = this->get_lo_idx(); res && i <= this->get_hi_idx();
                 ++i) {
                res = ((this->cpu_vecs[i]) == (other.cpu_vecs[i]));
            }
            return res;
        }
    }
};

template<uint32_t NCPU>
struct destructive_inner_iterator {
    uint64_t * cpu_vec;


    ~destructive_inner_iterator() noexcept = default;

    constexpr destructive_inner_iterator() noexcept : cpu_vec(NULL) {}

    destructive_inner_iterator(
        const destructive_inner_iterator<NCPU> & other) noexcept {
        cpu_vec = other.cpu_vec;
    }
    destructive_inner_iterator(const uint64_t & _cpu_vec) noexcept {
        cpu_vec = &_cpu_vec;
    }

    destructive_inner_iterator(uint64_t * const _cpu_vec) noexcept {
        cpu_vec = _cpu_vec;
    }

    destructive_inner_iterator(const var_cpu_set<NCPU> & _cpu_set) noexcept {
        cpu_vec = ((uint64_t *)(&(_cpu_set.cpu_vecs[0])));
    }


    constexpr uint32_t ALWAYS_INLINE
    get_first() noexcept {
        uint32_t next_cpu = bits::find_first_one<uint64_t>(this->cpu_vec[0]);
        this->cpu_vec[0] ^= ((1UL) << next_cpu);
        return next_cpu;
    }

    constexpr uint32_t ALWAYS_INLINE
    get_last() noexcept {
        uint32_t next_cpu = bits::find_last_one<uint64_t>(this->cpu_vec[0]);
        this->cpu_vec[0] ^= ((1UL) << next_cpu);
        return next_cpu;
    }


    constexpr uint32_t ALWAYS_INLINE
    operator++() noexcept {
        return this->get_first();
    }

    constexpr uint32_t ALWAYS_INLINE
    operator--() noexcept {
        return this->get_last();
    }

    constexpr uint32_t ALWAYS_INLINE
    operator++(int) noexcept {
        return this->get_first();
    }

    constexpr uint32_t ALWAYS_INLINE
    operator--(int) noexcept {
        return this->get_last();
    }


    constexpr bool ALWAYS_INLINE
    operator==(const destructive_inner_iterator other) const noexcept {
        return ((this->cpu_vec == NULL ? 0 : *(this->cpu_vec)) ==
                (other.cpu_vec == NULL ? 0 : *(other.cpu_vec)));
    }

    constexpr bool ALWAYS_INLINE
    operator!=(const destructive_inner_iterator other) const noexcept {
        return !(this[0] == other);
    }

    constexpr void ALWAYS_INLINE
    operator=(const destructive_inner_iterator other) noexcept {
        this->cpu_vec = other.cpu_vec;
    }

    constexpr void ALWAYS_INLINE
    operator=(const uint64_t _cpu_vec) noexcept {
        *(this->cpu_vec) = _cpu_vec;
    }

    constexpr void ALWAYS_INLINE
    operator=(const uint64_t * _cpu_vec) noexcept {
        this->cpu_vev = _cpu_vec;
    }

    constexpr bool ALWAYS_INLINE operator!() const noexcept {
        return !(*(this->cpu_vec));
    }
};


}  // namespace cpu
#endif
