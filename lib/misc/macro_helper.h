#ifndef _MACRO_HELPER_H_
#define _MACRO_HELPER_H_

//////////////////////////////////////////////////////////////////////
/* 
   This file contains helper macros for using __VA_ARGS__. Currently
   these macros are written so that 20 arguments is the maximum
   allowed. The formats are clear so expanding them to accept more
   argument should be simply enough.
*/


//////////////////////////////////////////////////////////////////////


//clang-format off
#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define PP_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0

//////////////////////////////////////////////////////////////////////
#define NOT_ONE_NARG(...) \
    PP_NARG_(__VA_ARGS__,NOT_ONE_RSEQ_N())

#define NOT_ONE_RSEQ_N()                            \
    MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,        \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,MANY,MANY,    \
        MANY,MANY,MANY,MANY,MANY,MANY,ONE,ONE
//////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////
//Helper for all apply/sum macro
#define PRIMITIVE_CAT(x, y) x ## y
#define CAT(x, y) PRIMITIVE_CAT(x, y)

//////////////////////////////////////////////////////////////////////
//Do macro on all args in __VA_ARGS__
#define APPLY(macro, OP, ...) CAT(APPLY_, PP_NARG(__VA_ARGS__))(macro, __VA_ARGS__)
#define APPLY_1(m, x1) m(x1)
#define APPLY_2(m, x1, x2) m(x1) OP m(x2)
#define APPLY_3(m, x1, x2, x3) m(x1) OP m(x2) OP m(x3)
#define APPLY_4(m, x1, x2, x3, x4) m(x1) OP m(x2) OP m(x3) OP m(x4)
#define APPLY_5(m, x1, x2, x3, x4, x5) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5)
#define APPLY_6(m, x1, x2, x3, x4, x5, x6) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6)
#define APPLY_7(m, x1, x2, x3, x4, x5, x6, x7) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7)
#define APPLY_8(m, x1, x2, x3, x4, x5, x6, x7, x8) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8)
#define APPLY_9(m, x1, x2, x3, x4, x5, x6, x7, x8, x9) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9)
#define APPLY_10(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10)
#define APPLY_11(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11)
#define APPLY_12(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12)
#define APPLY_13(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13)
#define APPLY_14(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14)
#define APPLY_15(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15)
#define APPLY_16(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15) OP m(x16)
#define APPLY_17(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15) OP m(x16) OP m(x17)
#define APPLY_18(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15) OP m(x16) OP m(x17) OP m(x18)
#define APPLY_19(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15) OP m(x16) OP m(x17) OP m(x18) OP m(x19)
#define APPLY_20(m, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20) m(x1) OP m(x2) OP m(x3) OP m(x4) OP m(x5) OP m(x6) OP m(x7) OP m(x8) OP m(x9) OP m(x10) OP m(x11) OP m(x12) OP m(x13) OP m(x14) OP m(x15) OP m(x16) OP m(x17) OP m(x18) OP m(x19) OP m(x20)

//clang-format on
#endif

