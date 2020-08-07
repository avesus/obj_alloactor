#ifndef _RSEQ_OPS_H_
#define _RSEQ_OPS_H_

#include <misc/cpp_attributes.h>
#include <stdint.h>
#include "rseq_asm_defs.h"
#include "rseq_base.h"


uint64_t NEVER_INLINE
try_reclaim_free_slots(uint64_t * v_cpu_ptr,
                       uint64_t * free_v_cpu_ptr_then_temp,
                       const uint32_t   start_cpu) {
    uint64_t ret_reclaimed_slots = 0;
    asm volatile(
        RSEQ_INFO_DEF(32)
        RSEQ_CS_ARR_DEF()
        RSEQ_PREP_CS_DEF()
        RSEQ_CMP_CUR_VS_START_CPUS()
        
        "movq (%[free_v_cpu_ptr_then_temp]), %[ret_reclaimed_slots]\n\t"  // get current free vec
        "testq %[ret_reclaimed_slots], %[ret_reclaimed_slots]\n\t"              // if is 0 nothing to do
        "jz 2f\n\t"
        "leaq -1(%[ret_reclaimed_slots]), %[free_v_cpu_ptr_then_temp]\n\t" // now temp
        "andq %[ret_reclaimed_slots], %[free_v_cpu_ptr_then_temp]\n\t"
        
        "xorq %[free_v_cpu_ptr_then_temp], (%[v_cpu_ptr])\n\t"       // xor bits
        "2:\n\t"
        RSEQ_START_ABORT_DEF()
        "movq $0, %[ret_reclaimed_slots]\n\t"
        "jmp 2b\n\t"
        RSEQ_END_ABORT_DEF()
        : [ ret_reclaimed_slots ] "+g"(ret_reclaimed_slots)
        : [ start_cpu ] "g"(start_cpu), 
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ free_v_cpu_ptr_then_temp ] "r"(free_v_cpu_ptr_then_temp)
        : "memory", "cc", "rax");
    return ret_reclaimed_slots;
}


uint64_t NEVER_INLINE
try_reclaim_all_free_slots(uint64_t * v_cpu_ptr,
                           uint64_t * free_v_cpu_ptr,
                           const uint32_t   start_cpu) {
    uint64_t ret_reclaimed_slots = 0;
    asm volatile(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        
        "movq (%[free_v_cpu_ptr]), %[ret_reclaimed_slots]\n\t"  // get current free vec
        "testq %[ret_reclaimed_slots], %[ret_reclaimed_slots]\n\t"              // if is 0 nothing to do
        "jz 2f\n\t"
        "xorq %[ret_reclaimed_slots], (%[v_cpu_ptr])\n\t"       // xor bits
        "2:\n\t"
        RSEQ_START_ABORT_DEF()
        "movq $0, %[ret_reclaimed_slots]\n\t"
        "jmp 2b\n\t"
        RSEQ_END_ABORT_DEF()
        : [ ret_reclaimed_slots ] "+r"(ret_reclaimed_slots)
        : [ start_cpu ] "g"(start_cpu), 
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ free_v_cpu_ptr ] "g"(free_v_cpu_ptr)
        : "memory", "cc", "rax");
    return ret_reclaimed_slots;
}


uint32_t NEVER_INLINE
or_if_unset(uint64_t * v_cpu_ptr, const uint64_t new_bit_mask, const uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        /* start critical section contents */
        "testq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "jnz 4f\n\t"
        "orq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "g"(start_cpu),
          [ new_bit_mask ] "r"(new_bit_mask),
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr)
        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}


uint32_t NEVER_INLINE
xor_if_set(uint64_t * v_cpu_ptr, const uint64_t new_bit_mask, const uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        /* start critical section contents */
        "testq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "jz 4f\n\t"
        "xorq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "g"(start_cpu),
          [ new_bit_mask ] "g"(new_bit_mask),
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr)
        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}

#endif
