// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "riscv_test.h"

void trap_entry();
void pop_tf(trapframe_t*);
void do_tohost(long tohost_value);

#define pa2kva(pa) ((void*)(pa) - MEGAPAGE_SIZE)

static uint64_t lfsr63(uint64_t x)
{
  uint64_t bit = (x ^ (x >> 1)) & 1;
  return (x >> 1) | (bit << 62);
}

static void cputchar(int x)
{
  do_tohost(0x0101000000000000 | (unsigned char)x);
}

static void cputstring(const char* s)
{
  while (*s)
    cputchar(*s++);
}

static void terminate(int code)
{
  do_tohost(code);
  while (1);
}

#define stringify1(x) #x
#define stringify(x) stringify1(x)
#define assert(x) do { \
  if (x) break; \
  cputstring("Assertion failed: " stringify(x) "\n"); \
  terminate(3); \
} while(0)

typedef struct { pte_t addr; void* next; } freelist_t;

pte_t l1pt[PTES_PER_PT] __attribute__((aligned(PGSIZE)));
pte_t user_l2pt[PTES_PER_PT] __attribute__((aligned(PGSIZE)));
pte_t user_l3pt[PTES_PER_PT] __attribute__((aligned(PGSIZE)));
pte_t kernel_l2pt[PTES_PER_PT] __attribute__((aligned(PGSIZE)));
pte_t kernel_l3pt[PTES_PER_PT] __attribute__((aligned(PGSIZE)));
freelist_t user_mapping[MAX_TEST_PAGES];
freelist_t freelist_nodes[MAX_TEST_PAGES];
freelist_t *freelist_head, *freelist_tail;

void printhex(uint64_t x)
{
  char str[17];
  for (int i = 0; i < 16; i++)
  {
    str[15-i] = (x & 0xF) + ((x & 0xF) < 10 ? '0' : 'a'-10);
    x >>= 4;
  }
  str[16] = 0;

  cputstring(str);
}

void evict(unsigned long addr)
{
  assert(addr >= PGSIZE && addr < MAX_TEST_PAGES * PGSIZE);
  addr = addr/PGSIZE*PGSIZE;

  freelist_t* node = &user_mapping[addr/PGSIZE];
  if (node->addr)
  {
    // check referenced and dirty bits
    assert(user_l3pt[addr/PGSIZE] & PTE_R);
    if (memcmp((void*)addr, pa2kva(addr), PGSIZE)) {
      assert(user_l3pt[addr/PGSIZE] & PTE_D);
      memcpy((void*)addr, pa2kva(addr), PGSIZE);
    }

    user_mapping[addr/PGSIZE].addr = 0;

    if (freelist_tail == 0)
      freelist_head = freelist_tail = node;
    else
    {
      freelist_tail->next = node;
      freelist_tail = node;
    }
  }
}

void handle_fault(unsigned long addr)
{
  assert(addr >= PGSIZE && addr < MAX_TEST_PAGES * PGSIZE);
  addr = addr/PGSIZE*PGSIZE;

  freelist_t* node = freelist_head;
  assert(node);
  freelist_head = node->next;
  if (freelist_head == freelist_tail)
    freelist_tail = 0;

  user_l3pt[addr/PGSIZE] = (node->addr >> PGSHIFT << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_URWX_SRW;
  asm volatile ("sfence.vm");

  assert(user_mapping[addr/PGSIZE].addr == 0);
  user_mapping[addr/PGSIZE] = *node;
  memcpy((void*)addr, pa2kva(addr), PGSIZE);

  __builtin___clear_cache(0,0);
}

static void do_vxcptrestore(long* where)
{
  vsetcfg(where[0]);
  vsetvl(where[1]);

  vxcpthold(&where[2]);

  // no hwachav4 restore yet
}

static void restore_vector(trapframe_t* tf)
{
  do_vxcptrestore(tf->hwacha_opaque);
}

void handle_trap(trapframe_t* tf)
{
  if (tf->cause == CAUSE_USER_ECALL)
  {
    int n = tf->gpr[10];

    for (long i = 1; i < MAX_TEST_PAGES; i++)
      evict(i*PGSIZE);

    terminate(n);
  }
  else if (tf->cause == CAUSE_FAULT_FETCH)
    handle_fault(tf->epc);
  else if (tf->cause == CAUSE_ILLEGAL_INSTRUCTION)
  {
    assert(tf->epc % 4 == 0);

    int* fssr;
    asm ("jal %0, 1f; fssr x0; 1:" : "=r"(fssr));

    if (*(int*)tf->epc == *fssr)
      terminate(1); // FP test on non-FP hardware.  "succeed."
    else
      assert(!"illegal instruction");
    tf->epc += 4;
  }
  else if (tf->cause == CAUSE_FAULT_LOAD || tf->cause == CAUSE_FAULT_STORE)
    handle_fault(tf->badvaddr);
  else if ((long)tf->cause < 0 && (uint8_t)tf->cause == IRQ_COP)
  {
    if (tf->hwacha_cause == HWACHA_CAUSE_VF_FAULT_FETCH ||
        tf->hwacha_cause == HWACHA_CAUSE_FAULT_LOAD ||
        tf->hwacha_cause == HWACHA_CAUSE_FAULT_STORE)
    {
      long badvaddr = vxcptaux();
      handle_fault(badvaddr);
    }
    else if(tf->hwacha_cause == HWACHA_CAUSE_ILLEGAL_CFG)
      assert(!"hwacha_ill_cfg");
    else if(tf->hwacha_cause == HWACHA_CAUSE_ILLEGAL_INSTRUCTION)
      assert(!"hwacha_ill_instr");
    else if(tf->hwacha_cause == HWACHA_CAUSE_PRIVILEGED_INSTRUCTION)
      assert(!"hwacha_priv_instr");
    else if(tf->hwacha_cause == HWACHA_CAUSE_TVEC_ILLEGAL_REGID)
      assert(!"hwacha_tvec_ill_regid");
    else if(tf->hwacha_cause == HWACHA_CAUSE_VF_MISALIGNED_FETCH)
      assert(!"hwacha_vf_misalign_fetch");
    else if(tf->hwacha_cause == HWACHA_CAUSE_VF_ILLEGAL_INSTRUCTION)
      assert(!"hwacha_vf_ill_instr");
    else if(tf->hwacha_cause == HWACHA_CAUSE_VF_ILLEGAL_REGID)
      assert(!"hwacha_vf_ill_regid");
    else if(tf->hwacha_cause == HWACHA_CAUSE_MISALIGNED_LOAD)
      assert(!"hwacha_misalign_load");
    else if(tf->hwacha_cause == HWACHA_CAUSE_MISALIGNED_STORE)
      assert(!"hwacha_misalign_store");
    else
      assert(!"unexpected interrupt");
  }
  else
    assert(!"unexpected exception");

out:
  if (!(tf->sr & SSTATUS_PS) && (tf->sr & SSTATUS_XS))
    restore_vector(tf);
  pop_tf(tf);
}

static void coherence_torture()
{
  // cause coherence misses without affecting program semantics
  uint64_t random = ENTROPY;
  while (1) {
    uintptr_t paddr = (random % (2 * (MAX_TEST_PAGES + 1) * PGSIZE)) & -4;
#ifdef __riscv_atomic
    if (random & 1) // perform a no-op write
      asm volatile ("amoadd.w zero, zero, (%0)" :: "r"(paddr));
    else // perform a read
#endif
      asm volatile ("lw zero, (%0)" :: "r"(paddr));
    random = lfsr63(random);
  }
}

void vm_boot(long test_addr, long seed)
{
  if (read_csr(mhartid) > 0)
    coherence_torture();

  assert(SIZEOF_TRAPFRAME_T == sizeof(trapframe_t));

#if MAX_TEST_PAGES > PTES_PER_PT
# error
#endif
  // map kernel to uppermost megapage
  l1pt[PTES_PER_PT-1] = ((pte_t)kernel_l2pt >> PGSHIFT << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_TABLE;
  kernel_l2pt[PTES_PER_PT-1] = ((pte_t)kernel_l3pt >> PGSHIFT << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_TABLE;

  // map user to lowermost megapage
  l1pt[0] = ((pte_t)user_l2pt >> PGSHIFT << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_TABLE;
  user_l2pt[0] = ((pte_t)user_l3pt >> PGSHIFT << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_TABLE;
  write_csr(sptbr, l1pt);

  // set up supervisor trap handling
  write_csr(stvec, pa2kva(trap_entry));
  write_csr(sscratch, pa2kva(read_csr(mscratch)));
  // interrupts on; FPU on; accelerator on
  set_csr(mstatus, MSTATUS_IE1 | MSTATUS_FS | MSTATUS_XS);
  // virtual memory off; set user mode upon eret
  clear_csr(mstatus, MSTATUS_VM | MSTATUS_PRV1);
  // virtual memory to Sv39
  set_csr(mstatus, (long)VM_SV39 << __builtin_ctzl(MSTATUS_VM));

  seed = 1 + (seed % MAX_TEST_PAGES);
  freelist_head = pa2kva((void*)&freelist_nodes[0]);
  freelist_tail = pa2kva(&freelist_nodes[MAX_TEST_PAGES-1]);
  for (long i = 0; i < MAX_TEST_PAGES; i++)
  {
    freelist_nodes[i].addr = (MAX_TEST_PAGES + seed)*PGSIZE;
    freelist_nodes[i].next = pa2kva(&freelist_nodes[i+1]);
    seed = LFSR_NEXT(seed);

    kernel_l3pt[i] = (i << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_SRWX;
  }
  freelist_nodes[MAX_TEST_PAGES-1].next = 0;

  trapframe_t tf;
  memset(&tf, 0, sizeof(tf));
  write_csr(mepc, test_addr);
  pop_tf(&tf);
}
