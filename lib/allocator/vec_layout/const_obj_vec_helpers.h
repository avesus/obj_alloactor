#ifndef _CONST_OBJ_VEC_HELPERS_H_
#define _CONST_OBJ_VEC_HELPERS_H_

//////////////////////////////////////////////////////////////////////
// constexpr functions for computing various compile time info based on template
// values. These should all be entirely computable at compile time.

template<uint32_t... per_level_nvecs>
static constexpr uint32_t
_get_nvecs(const uint32_t n) {
    int32_t temp[] = { static_cast<int32_t>(per_level_nvecs)... };
    return temp[n];
}
static_assert(_get_nvecs<1, 2, 3>(0) == 1);
static_assert(_get_nvecs<1, 2, 3>(1) == 2);
static_assert(_get_nvecs<1, 2, 3>(2) == 3);
static_assert(_get_nvecs<1, 2, 3, 1>(3) == 1);

template<uint32_t... per_level_nvecs>
static constexpr uint64_t
_calculate_alloc_arr_size(const uint32_t n) {
    return n ? (64 * _get_nvecs<per_level_nvecs...>(n) *
                _calculate_alloc_arr_size<per_level_nvecs...>(n - 1))
             : _get_nvecs<per_level_nvecs...>(0);
}
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(0) == 1);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(1) == 1 * 2 * 64);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(2) == 1 * 2 * 3 * 64 * 64);
static_assert(_calculate_alloc_arr_size<1, 2, 3, 4>(3) ==
              1 * 2 * 3 * 4 * 64 * 64 * 64);


static constexpr uint64_t
_calculate_free_arr_size(const uint32_t n) {
    return n ? 64 * 8 * _calculate_free_arr_size(n - 1) : 8;
}
static_assert(_calculate_free_arr_size(0) == 8);
static_assert(_calculate_free_arr_size(1) == 8 * 8 * 64);
static_assert(_calculate_free_arr_size(2) == 8 * 8 * 8 * 64 * 64);
static_assert(_calculate_free_arr_size(3) == 8 * 8 * 8 * 8 * 64 * 64 * 64);

template<uint32_t... per_level_nvec>
static constexpr uint64_t
_total_alloc_arr_size(const uint32_t n) {
    return n ? _calculate_alloc_arr_size<per_level_nvec...>(n - 1) +
                   _total_alloc_arr_size<per_level_nvec...>(n - 1)
             : 0;
}
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(0) == 0);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(1) == 1);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(2) == 1 + 1 * 2 * 64);
static_assert(_total_alloc_arr_size<1, 2, 3, 4>(3) ==
              1 + 1 * 2 * 64 + 1 * 2 * 3 * 64 * 64);


static constexpr uint64_t
_total_free_arr_size(const uint32_t n) {
    return n ? _calculate_free_arr_size(n - 1) + _total_free_arr_size(n - 1)
             : 0;
}

static_assert(_total_free_arr_size(0) == 0);
static_assert(_total_free_arr_size(1) == 8);
static_assert(_total_free_arr_size(2) == 8 + 8 * 8 * 64);
static_assert(_total_free_arr_size(3) == 8 + 8 * 8 * 64 + 8 * 8 * 8 * 64 * 64);


template<typename T, uint32_t... per_level_nvec>
static constexpr uint64_t
_region_size(const uint32_t n) {
    return 64 * sizeof(T) * _calculate_alloc_arr_size<per_level_nvec...>(n);
}
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(0) ==
              1 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(1) ==
              1 * 2 * 64 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(2) ==
              1 * 2 * 3 * 64 * 64 * 64 * sizeof(uint64_t));
static_assert(_region_size<uint64_t, 1, 2, 3, 4>(3) ==
              1 * 2 * 3 * 4 * 64 * 64 * 64 * 64 * sizeof(uint64_t));


#endif
