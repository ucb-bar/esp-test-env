// See LICENSE for license details.

#ifndef _ENV_VIRTUAL_SINGLE_CORE_H
#define _ENV_VIRTUAL_SINGLE_CORE_H

#include "../p/riscv_test.h"

//-----------------------------------------------------------------------
// Begin Macro
//-----------------------------------------------------------------------

#undef RVTEST_FP_ENABLE
#define RVTEST_FP_ENABLE fssr x0

#undef RVTEST_RV64UV
#define RVTEST_RV64UV                                                   \
        RVTEST_RV64UF

#undef RVTEST_CODE_BEGIN
#define RVTEST_CODE_BEGIN                                               \
        .text;                                                          \
        .global userstart;                                              \
userstart:                                                              \
        init

//-----------------------------------------------------------------------
// Pass/Fail Macro
//-----------------------------------------------------------------------

#undef RVTEST_PASS
#define RVTEST_PASS li a0, 1; scall

#undef RVTEST_FAIL
#define RVTEST_FAIL sll a0, TESTNUM, 1; 1:beqz a0, 1b; or a0, a0, 1; scall;

//-----------------------------------------------------------------------
// Data Section Macro
//-----------------------------------------------------------------------

#undef RVTEST_DATA_END
#define RVTEST_DATA_END

//-----------------------------------------------------------------------
// Supervisor mode definitions and macros
//-----------------------------------------------------------------------

#define vxcptret() ({ \
          asm volatile ("vxcptret"); })

#define vxcptkill() ({ \
          asm volatile ("vxcptkill"); })

#define vfence_vma() ({ \
          asm volatile ("vfence.vma"); })

#define MAX_TEST_PAGES 63 // this must be the period of the LFSR below
#define LFSR_NEXT(x) (((((x)^((x)>>1)) & 1) << 5) | ((x) >> 1))

#define PGSHIFT 12
#define PGSIZE (1UL << PGSHIFT)

# define SIZEOF_TRAPFRAME_T ((__riscv_xlen / 8) * (36+3))
# define SIZEOF_TRAPFRAME_T_SCALAR ((__riscv_xlen / 8) * (36+1))

#ifndef __ASSEMBLER__

static inline void vsetcfg(long cfg)
{
  asm volatile ("vsetcfg %0" : : "r"(cfg));
}

static inline void vsetvl(long vl)
{
  long __tmp;
  asm volatile ("vsetvl %0,%1" : "=r"(__tmp) : "r"(vl));
}

static inline long vgetcfg()
{
  int cfg;
  asm volatile ("vgetcfg %0" : "=r"(cfg) :);
  return cfg;
}

static inline long vgetvl()
{
  int vl;
  asm volatile ("vgetvl %0" : "=r"(vl) :);
}

static inline long vxcptval()
{
  long aux;
  asm volatile ("vxcptval %0" : "=r"(aux) :);
  return aux;
}

static inline long vxcptcause()
{
  long cause;
  asm volatile ("vxcptcause %0" : "=r"(cause) :);
  return cause;
}

static inline long vxcptpc()
{
  long epc;
  asm volatile ("vxcptpc %0" : "=r"(epc) :);
  return epc;
}

typedef unsigned long pte_t;
#define LEVELS (sizeof(pte_t) == sizeof(uint64_t) ? 3 : 2)
#define PTIDXBITS (PGSHIFT - (sizeof(pte_t) == 8 ? 3 : 2))
#define VPN_BITS (PTIDXBITS * LEVELS)
#define VA_BITS (VPN_BITS + PGSHIFT)
#define PTES_PER_PT (1UL << RISCV_PGLEVEL_BITS)
#define MEGAPAGE_SIZE (PTES_PER_PT * PGSIZE)

typedef struct
{
  long gpr[32];
  long sr;
  long epc;
  long badvaddr;
  long cause;
  long hwacha_cause;
  long hwacha_opaque[2];
} trapframe_t;
#endif

#endif
