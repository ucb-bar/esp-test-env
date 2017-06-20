// See LICENSE for license details.

#ifndef _ENV_VIRTUAL_PREFAULT_SINGLE_CORE_H
#define _ENV_VIRTUAL_PREFAULT_SINGLE_CORE_H

#include "../v/riscv_test.h"

//-----------------------------------------------------------------------
// Begin Macro
//-----------------------------------------------------------------------
#undef RVTEST_CODE_BEGIN
#define RVTEST_CODE_BEGIN                                               \
        .text;                                                          \
        .align  13;                                                     \
        .global userstart;                                              \
userstart:                                                              \
        la t0, begin_data;                                              \
        la t1, end_data;                                                \
prefault_data:                                                          \
        lb t2, 0(t0); /* load each data byte to fault before the test*/ \
        sb t2, 0(t0); /* store each data byte to give us write perms*/  \
        addi t0, t0, 1;                                                 \
        blt t0, t1, prefault_data;                                      \
        la t0, userstart;                                               \
        la t1, end_inst;                                                \
prefault_inst:                                                          \
        lb t2, 0(t0); /* load each inst byte to fault before the test*/ \
        addi t0, t0, 1;                                                 \
        blt t0, t1, prefault_inst;                                      \
        init

//-----------------------------------------------------------------------
// End Macro
//-----------------------------------------------------------------------
#undef RVTEST_CODE_END
#define RVTEST_CODE_END                                                 \
.global end_inst; end_inst:                                             \
ecall:  ecall;                                                          \
        j ecall

//-----------------------------------------------------------------------
// Data Section Macro
//-----------------------------------------------------------------------

#undef RVTEST_DATA_BEGIN
#define RVTEST_DATA_BEGIN .global begin_data; begin_data:

#undef RVTEST_DATA_END
#define RVTEST_DATA_END .global end_data; end_data:


#endif
