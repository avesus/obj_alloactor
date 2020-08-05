#ifndef _ATOMIC_UTILS_H_
#define _ATOMIC_UTILS_H_
#include <stdint.h>
#include <atomic>
#include <type_traits>
namespace atomics {

enum safety { safe = 1, unsafe = 0 };

template<typename T, safety s>
using atomic_t = typename std::conditional<s == safe, volatile T, T>::type;


};  // namespace atomics

#endif
