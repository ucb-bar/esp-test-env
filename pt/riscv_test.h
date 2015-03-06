// See LICENSE for license details.

#ifndef _ENV_PHYSICAL_SINGLE_CORE_TIMER_H
#define _ENV_PHYSICAL_SINGLE_CORE_TIMER_H

#include "../p/riscv_test.h"

#undef EXTRA_INIT_TIMER
#define EXTRA_INIT_TIMER                                                \
  ENABLE_TIMER_INTERRUPT;                                               \
  j 6f;                                                                 \
  XCPT_HANDLER;                                                         \
6:

//-----------------------------------------------------------------------
// Data Section Macro
//-----------------------------------------------------------------------

#undef EXTRA_DATA
#define EXTRA_DATA                 \
        .align 3;                  \
regspill:                          \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
        .dword 0xdeadbeefcafebabe; \
evac:                              \
        .skip 32768;               \

//-----------------------------------------------------------------------
// Misc
//-----------------------------------------------------------------------

#define ENABLE_TIMER_INTERRUPT \
        csrw clear_ipi,x0;           \
        csrr a0,status;              \
        li a1,SR_IM;                 \
        or a0,a0,a1;                 \
        csrw status,a0;              \
        la a0,_handler;              \
        csrw evec,a0;                \
        csrw count,x0;               \
        addi a0,x0,100;              \
        csrw compare,a0;             \
        csrs status,SR_EI;           \

#define XCPT_HANDLER \
_handler: \
        csrw sup0,a0;                \
        csrw sup1,a1;                \
        csrr a0,cause;               \
        li a1,CAUSE_SYSCALL;         \
        bne a0,a1,_cont;             \
        li a1,1;                     \
        bne x27,a1,_fail;            \
_pass: \
        fence;                       \
        csrw tohost, 1;              \
1:      j 1b;                        \
_fail: \
        fence;                       \
        beqz TESTNUM, 1f;            \
        sll TESTNUM, TESTNUM, 1;     \
        or TESTNUM, TESTNUM, 1;      \
        csrw tohost, TESTNUM;        \
1:      j 1b;                        \
_cont: \
        vxcptcause x0;               \
        la a0,evac;                  \
        vxcptsave a0;                \
        vxcptrestore a0;             \
        j _exit;                     \
                                     \
_exit: \
        csrs status,SR_PEI;          \
        csrc status,SR_PS;           \
        csrr a0,count;               \
        addi a0,a0,100;              \
        csrw compare,a0;             \
        csrr a0,sup0;                \
        csrr a1,sup1;                \
        sret;                        \

#undef RVTEST_PASS
#define RVTEST_PASS \
        li x27, 1; \
        scall; \

#undef RVTEST_FAIL
#define RVTEST_FAIL \
        li x27,2; \
        scall; \

#endif
