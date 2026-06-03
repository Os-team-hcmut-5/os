/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  int old_sbrk = cur_vma->sbrk;

  /* INCREASE THE LIMIT
   * SYSCALL 1 sys_memmap
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = size;
#else
  regs.a3 = PAGING_PAGE_ALIGNSZ(size);
#endif  
  int ret = _syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */
  if (ret < 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  /*Successful increase limit */
  caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);

  if (rgnode->rg_start == 0 && rgnode->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  if (freerg_node == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->vmaid = vmaid;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  /*  dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
printf("%s:%d\n",__func__,__LINE__);
#ifdef IODUMP
  /* dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{

#ifdef MM64
  uint64_t pte = pte_get_entry(caller, pgn);
#else
  uint32_t pte = pte_get_entry(caller, pgn);
#endif

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    addr_t vicpgn, swpfpn, vicfpn, tgtfpn;
  #ifdef MM64
      uint64_t vicpte;
  #else
      uint32_t vicpte;
  #endif
    
    if (MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) != 0)
    {
      /* Play with your paging theory here */
      /* Find victim page */
      if (find_victim_page(caller->krnl->mm, &vicpgn) == -1)
      {
        return -1;
      }
      /* Get free frame in MEMSWP */
      if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
      {
        return -1;
      }
      vicpte = pte_get_entry(caller, vicpgn);
      vicfpn = PAGING_FPN(vicpte);
      tgtfpn = vicfpn;
      /*  Initialize the target frame storing our variable */
      struct sc_regs regs;
      regs.a1 = 1;
      regs.a2 = vicfpn;
      regs.a3 = swpfpn;
      _syscall(caller->krnl, caller->pid, 17, &regs);

      pte_set_swap(&caller->krnl->mm->pgd[vicpgn], 0, swpfpn);
    }
    if (pte != 0)
    {
      addr_t old_swpfpn = PAGING_SWP(pte);
      /* Implement swap frame from MEMRAM to MEMSWP and vice versa*/
      struct sc_regs regs;
      regs.a1 = 1;
      regs.a2 = old_swpfpn;
      regs.a3 = tgtfpn;
      _syscall(caller->krnl, caller->pid, 17, &regs);
      MEMPHY_put_freefp(caller->krnl->active_mswp, old_swpfpn);
    }
    /* Update page table */
    pte_set_fpn(&caller->krnl->mm->pgd[pgn], tgtfpn);
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
  }
  *fpn = PAGING_FPN(pte_get_entry(caller, pgn));
  return 0;

}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /*  
   *  MEMPHY_read(caller->krnl->mram, phyaddr, data);
   *  MEMPHY READ 
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
  */

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  regs.a3 = (addr_t)data;

  _syscall(caller->krnl, caller->pid, 17, &regs);
   

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */


  /*  
   *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
   *  MEMPHY WRITE with SYSMEM_IO_WRITE 
   * SYSCALL 17 sys_memmap
  */
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE; /* The command: Write to IO */
  regs.a2 = phyaddr;         /* The destination: Exact physical address in RAM */
  regs.a3 = (addr_t)value;   /* The data: Pass the raw byte to store */
  _syscall(caller->krnl, caller->pid, 17, &regs);
  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  /* Invalid memory identify */
  if (currg == NULL || cur_vma == NULL) 
  {
    return -1;
  }
  if (currg->rg_start + offset >= currg->rg_end) 
  {
    return -1; /* Segmentation Fault! User is trying to read out of bounds. */
  }
  return pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller);
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
printf("%s:%d\n",__func__,__LINE__);
  int val = __read(proc, 0, source, offset, &data);
  if (val == 0)
    *destination = data;

#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (currg->rg_start + offset >= currg->rg_end) 
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1; /* Segmentation Fault! */
  }

  int ret = pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return ret;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}


/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t * caller, uint32_t size, uint32_t reg_index)
{
  /* provide OS level management
   *       and forward the request to helper
   */
  addr_t  addr;
  int val = __kmalloc(caller, -1, reg_index, size, &addr);

  /* provide OS kmem allocation validation
   */

  if (val != 0) {
    return -1; /* Kernel allocation failed */
  }

  return 0;
}


/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /* provide OS kernel memory allocation
   *       update krnl_pgd for OS kernel level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->symrgtbl...
  //krnl->krnl_pgd ...
  pthread_mutex_lock(&mmvm_lock);
  struct krnl_t *krnl = caller->krnl;
  addr_t tgtfpn;
  // Grab a physical frame directly from RAM.
  if (MEMPHY_get_freefp(krnl->mram, &tgtfpn) != 0) 
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1; /* Out of physical RAM*/
  }

  // Calculate the Virtual Page Number we want to assign
  int pgn = rgid;
  pte_set_fpn(&krnl->krnl_pgd[pgn], tgtfpn);
  
  krnl->symrgtbl[rgid].rg_start = pgn * PAGING_PAGESZ; 
  krnl->symrgtbl[rgid].rg_end = (pgn * PAGING_PAGESZ) + size;

  *alloc_addr = krnl->symrgtbl[rgid].rg_start;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
  /* provide OS level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...
  pthread_mutex_lock(&mmvm_lock);
  
  struct krnl_t *krnl = caller->krnl;
  krnl->kcpooltbl[cache_pool_id].pool_sz = size;
  krnl->kcpooltbl[cache_pool_id].slot_sz = align;
  krnl->kcpooltbl[cache_pool_id].free_head = 0;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
  /* provide OS level management
   *       and forward the request to helper
   */
  addr_t addr;
  /* Call the internal engine. vmaid is -1 because cache pools bypass standard VMAs */
  int val = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &addr);

  if (val != 0) return -1;
  return 0;
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
  /* provide OS level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->symrgtbl...
  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...

  pthread_mutex_lock(&mmvm_lock);
  
  struct krnl_t *krnl = caller->krnl;

  addr_t slot_size = krnl->kcpooltbl[cache_pool_id].slot_sz;
  addr_t current_head = krnl->kcpooltbl[cache_pool_id].free_head;
  
  if (current_head + slot_size > krnl->kcpooltbl[cache_pool_id].pool_sz) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1; 
  }

  /* Map this specific slot to the requested Symbol Region ID */
  krnl->symrgtbl[rgid].rg_start = current_head;
  krnl->symrgtbl[rgid].rg_end = current_head + slot_size;

  /* Update the pool to point to the next available slot */
  krnl->kcpooltbl[cache_pool_id].free_head += slot_size;

  /* Hand the address back */
  *alloc_addr = krnl->symrgtbl[rgid].rg_start;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;

}


int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  /* provide OS level management kmem
   */
  /*
   * Map kernel address range
   */
  //__read_user_mem(...)
  //__write_kernel_mem(...);

  BYTE data;
  
  for (uint32_t i = 0; i < size; i++) 
  {
    if (__read_user_mem(caller, 0, source, offset + i, &data) != 0) {
      return -1; 
    }

    if (__write_kernel_mem(caller, -1, destination, i, data) != 0) {
      return -1; 
    }
  }

  return 0;
  return 0;
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  /* provide OS level management kmem
   */
  /*
   * Map kernel address range
   */
  //__read_kernel_mem(...)
  //__write_user_mem(...);

  BYTE data;
  
  for (uint32_t i = 0; i < size; i++) 
  {
    if (__read_kernel_mem(caller, -1, source, i, &data) != 0) {
      return -1; 
    }

    if (__write_user_mem(caller, 0, destination, offset + i, data) != 0) {
      return -1; 
    }
  }

  return 0;
}


/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* provide OS memory operator for kernel memory region */
  //krnl->krnl_pgd ... or krnl->pgd ... based on kmem implementation strategy
  struct krnl_t *krnl = caller->krnl;
  struct vm_rg_struct *currg = &krnl->symrgtbl[rgid];

  if (currg == NULL || currg->rg_start + offset >= currg->rg_end) 
    return -1;

  addr_t v_addr = currg->rg_start + offset;
  
  addr_t pgn = PAGING_PGN(v_addr);
  addr_t off = PAGING_OFFST(v_addr);

#ifdef MM64
  uint64_t pte = krnl->krnl_pgd[pgn];
#else
  uint32_t pte = krnl->krnl_pgd[pgn];
#endif

  if (!PAGING_PAGE_PRESENT(pte)) 
    return -1; /* Kernel memory should never be swapped out! */

  addr_t fpn = PAGING_FPN(pte);
  addr_t phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  
  MEMPHY_read(krnl->mram, phyaddr, data);

  return 0;
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  struct krnl_t *krnl = caller->krnl;
  struct vm_rg_struct *currg = &krnl->symrgtbl[rgid];

  if (currg == NULL || currg->rg_start + offset >= currg->rg_end) 
    return -1;

  addr_t v_addr = currg->rg_start + offset;
  
  addr_t pgn = PAGING_PGN(v_addr);
  addr_t off = PAGING_OFFST(v_addr);

#ifdef MM64
  uint64_t pte = krnl->krnl_pgd[pgn];
#else
  uint32_t pte = krnl->krnl_pgd[pgn];
#endif

  if (!PAGING_PAGE_PRESENT(pte)) 
    return -1; 

  addr_t fpn = PAGING_FPN(pte);
  addr_t phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  
  MEMPHY_write(krnl->mram, phyaddr, value);

  return 0;
}

/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* provide OS level management user memory access */
  //krnl->pgd ...
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  
  if (currg == NULL || currg->rg_start + offset >= currg->rg_end) 
    return -1;

  return pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller);
}


/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  /* provide OS level management user memory access */
  //krnl->pgd ...

  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  
  if (currg == NULL || currg->rg_start + offset >= currg->rg_end) 
    return -1;

  return pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller);
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  
  addr_t pagenum, fpn;
  
#ifdef MM64
  uint64_t pte;
#else
  uint32_t pte;
#endif

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->krnl->mm->pgd[pagenum];

    if (pte == 0) continue; 

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      /* Secondary check to ensure we only free valid swap frames */
      if (fpn != 0) { 
        MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
      }
    }
    
    /* Clear the dictionary entry so it cannot be double-freed */
    caller->krnl->mm->pgd[pagenum] = 0;
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  if (!pg)
  {
    return -1;
  }
  
  struct pgn_t *prev = NULL;
  while (pg->pg_next)
  {
    prev = pg;
    pg = pg->pg_next;
  }
  
  *retpgn = pg->pgn;
  
  if (prev == NULL) 
  {
    mm->fifo_pgn = NULL; 
  } else 
  {
    prev->pg_next = NULL;
  }

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, addr_t size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  newrg->rg_start = newrg->rg_end = (addr_t)-1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { 
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { 
        struct vm_rg_struct *nextrg = rgit->rg_next;

        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;
          rgit->rg_next = nextrg->rg_next;
          free(nextrg);
        }
        else
        {                                
          rgit->rg_start = rgit->rg_end; 
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; 
    }
  }

  if (newrg->rg_start == (addr_t)-1) 
    return -1;

  return 0;
}

// #endif
