#ifndef _SYS_INFO_H_
#define _SYS_INFO_H_
#define HAVE_CONST_SYS_INFO

// Number of processors (this is cores includes hyperthreads)
#define NPROCS 8

// Number of physical cores
#define NCORES 4

// Logical to physical core lookup
#define PHYS_CORE(X) get_phys_core(X)
uint32_t inline __attribute__((always_inline)) __attribute__((const))
get_phys_core(const uint32_t logical_core_num) {
	static const constexpr uint32_t core_map[NPROCS] = { 0, 1, 2, 3, 0, 1, 2, 3 };
	return core_map[logical_core_num];
}

// Virtual memory page size
#define PAGE_SIZE 4096

// Virtual memory address space bits
#define VM_NBITS 48

// Physical memory address space bits
#define PHYS_M_NBITS 39

// cache line size (should be same for L1, L2, and L3)
#define CACHE_LINE_SIZE 64

#endif
