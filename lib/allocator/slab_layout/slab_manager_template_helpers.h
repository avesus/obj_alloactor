#ifndef _SLAB_MANAGER_TEMPLATE_HELPER_H_
#define _SLAB_MANAGER_TEMPLATE_HELPER_H_

//////////////////////////////////////////////////////////////////////
// some template bull shit for make recursive type base on level argument

template<typename... Vals>
constexpr int32_t
get_N(int32_t n, Vals... vals) {
    int32_t temp[] = { static_cast<int32_t>(vals)... };
    return temp[n];
}

template<typename T, int32_t nlevels, int32_t level, int32_t... per_level_nvec>
struct type_helper;


template<typename T, int32_t nlevel, int32_t... per_level_nvec>
struct type_helper<T, nlevel, nlevel, per_level_nvec...> {
    typedef slab<T, get_N(nlevel, per_level_nvec...)> type;
};

template<typename T, int32_t nlevels, int32_t level, int32_t... per_level_nvec>
struct type_helper {
    typedef super_slab<
        T,
        get_N(level, per_level_nvec...),
        typename type_helper<T, nlevels, level + 1, per_level_nvec...>::type>
        type;
};


#endif
