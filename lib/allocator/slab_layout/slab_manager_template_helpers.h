#ifndef _SLAB_MANAGER_TEMPLATE_HELPER_H_
#define _SLAB_MANAGER_TEMPLATE_HELPER_H_

//////////////////////////////////////////////////////////////////////
// some template bull shit for make recursive type base on level argument

template<uint32_t... per_level_nvec>
constexpr uint32_t
nlevel_specified(uint32_t n) {
    uint32_t temp[] = { static_cast<uint32_t>(per_level_nvec)... };
    return sizeof(temp) / sizeof(uint32_t);
}


template<uint32_t... per_level_nvec>
constexpr uint32_t
get_N(uint32_t n) {
    uint32_t temp[] = { static_cast<uint32_t>(per_level_nvec)... };
    return temp[n];
}

template<typename T,
         uint32_t nlevels,
         uint32_t level,
         uint32_t... per_level_nvec>
struct type_helper;


template<typename T, uint32_t nlevel, uint32_t... per_level_nvec>
struct type_helper<T, nlevel, nlevel, per_level_nvec...> {
    typedef obj_slab<T, get_N<per_level_nvec...>(nlevel)> type;
};

template<typename T,
         uint32_t nlevels,
         uint32_t level,
         uint32_t... per_level_nvec>
struct type_helper {
    typedef super_slab<
        T,
        get_N<per_level_nvec...>(level),
        typename type_helper<T, nlevels, level + 1, per_level_nvec...>::type>
        type;
};


template<typename T1, typename T2>
struct same_base_type : std::is_same<T1, T2> {};

template<template<typename...> typename base_T,
         typename inner_T1,
         typename inner_T2>
struct same_base_type<base_T<inner_T1>, base_T<inner_T2>> : std::true_type {};

#endif
