/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/*  implement the page direactories mapping */

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}

/*
 * walk_page_table - Traverses the 64-bit Radix Tree (Skeleton Compliant)
 * @krnl  : kernel pointer
 * @pgn   : page number
 * @alloc : 1 if we should allocate missing branches, 0 if we should fail
 * Returns a pointer to the actual PTE, or NULL if it doesn't exist.
 */
addr_t* walk_page_table(struct mm_struct *mm, addr_t pgn, int alloc) 
{
  addr_t pgd, p4d, pud, pmd, pt;

#ifdef MM64
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

  if (mm->pgd[pgd] == 0) {
      if (!alloc) return NULL;
      mm->pgd[pgd] = (addr_t)calloc(512, sizeof(addr_t));
  }
  mm->p4d = (addr_t *)mm->pgd[pgd]; 

  if (mm->p4d[p4d] == 0) {
      if (!alloc) return NULL;
      mm->p4d[p4d] = (addr_t)calloc(512, sizeof(addr_t));
  }
  mm->pud = (addr_t *)mm->p4d[p4d];

  if (mm->pud[pud] == 0) {
      if (!alloc) return NULL;
      mm->pud[pud] = (addr_t)calloc(512, sizeof(addr_t));
  }
  mm->pmd = (addr_t *)mm->pud[pud];

  if (mm->pmd[pmd] == 0) {
      if (!alloc) return NULL;
      mm->pmd[pmd] = (addr_t)calloc(512, sizeof(addr_t));
  }
  mm->pt = (addr_t *)mm->pmd[pmd];

  return &mm->pt[pt];

#else
  /* Legacy 32-bit fallback */
  return (addr_t *)&mm->pgd[pgn];
#endif
}


/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{

  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return -1; 
  }

  /* Perform multi-level page mapping */
  addr_t *pte = walk_page_table(caller->mm, pgn, 1);
  if (pte == NULL) return -1;
  
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
  
  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return -1; 
  }

  addr_t *pte = walk_page_table(caller->mm, pgn, 1);
  if (pte == NULL) return -1;
  
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
  return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return 0; 
  }
  
  addr_t *pte = walk_page_table(caller->mm, pgn, 0);
  
  if (pte == NULL) {
      return 0; 
  }

  return (uint32_t)(*pte);
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return -1; 
  }

  addr_t *pte = walk_page_table(caller->mm, pgn, 1);
  if (pte == NULL) return -1;

  *pte = pte_val;
  
  return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 * dummy allocation with pattern
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  int pgit = 0;
  uint32_t pattern = 0xdeadbeef;
  addr_t pgn;

  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return -1; 
  }

  //  memset the page table with given pattern
  addr_t start_pgn = PAGING_PGN(addr);
  
  for (pgit = 0; pgit < pgnum; pgit++)
  {
    pgn = start_pgn + pgit;
    pte_set_entry(caller, pgn, pattern);
  }


  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  struct framephy_struct *fpit = frames;
  int pgit = 0;
  addr_t pgn;

  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL || frames == NULL || ret_rg == NULL) {
      return -1; 
  }

  addr_t size = pgnum * PAGING_PAGESZ;
  // overflow check
  if (addr + size < addr) 
  {
    return -3000; 
  }
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + size;
  ret_rg->vmaid = 0;
  // get the starting page number
  addr_t start_pgn = PAGING_PGN(addr); 

  for (pgit = 0; pgit < pgnum; pgit++)
  {
    if (fpit == NULL) break;
    pgn = start_pgn + pgit;

    // map the virtual page to the physical frame
    pte_set_fpn(caller, pgn, fpit->fpn);
    
    /* Enqueue the page into the FIFO tracking system */
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);

    /* Move to the next physical frame in the linked list */
      fpit = fpit->fp_next;
  }

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{

  addr_t fpn;
  int pgit;
  struct framephy_struct *newfp_str = NULL;
  struct framephy_struct *tail = NULL;

  if (caller == NULL || caller->krnl == NULL || frm_lst == NULL || req_pgnum <= 0) {
      return -1;
  }

  // allocating physical ram to frm_lst
  *frm_lst = NULL;
  
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
      newfp_str = (struct framephy_struct *)malloc(sizeof(struct framephy_struct));
      newfp_str->fpn = fpn;
      newfp_str->fp_next = NULL;

      if (*frm_lst == NULL)
      {
        *frm_lst = newfp_str;
        tail = newfp_str;
      }
      else
      {
        tail->fp_next = newfp_str;
        tail = newfp_str;
      }
    }
    else
    {
      // out of memory, give back allocated frames
      struct framephy_struct *temp = *frm_lst;
      struct framephy_struct *next_node;

      while (temp != NULL)
      {
        next_node = temp->fp_next;
        free(temp);
        temp = next_node;
      }

      *frm_lst = NULL;
      return -3000;
    }
  }

  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
  int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
    return -1;
  
  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  if (caller == NULL) return -1;
  caller->mm = mm;
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  if (vma0 == NULL) return -1;
  // 64-bit demand allocation
  caller->mm->pgd = (addr_t *)calloc(512, sizeof(addr_t));
  caller->mm->p4d = NULL;
  caller->mm->pud = NULL;
  caller->mm->pmd = NULL;
  caller->mm->pt = NULL;

  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;

  // setup the free region list
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  vma0->vm_freerg_list = NULL;
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  // link the vma back to the memory manager
  vma0->vm_next = NULL;
  vma0->vm_mm = mm;

  // update mmap
  caller->mm->mmap = vma0;
  caller->mm->fifo_pgn = NULL;
  caller->mm->kcpooltbl = NULL;
  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) 
  {
    caller->mm->symrgtbl[i].rg_start = 0;
    caller->mm->symrgtbl[i].rg_end = 0;
    caller->mm->symrgtbl[i].rg_next = NULL; 
  }

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("\n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  printf("print_pgtbl:\n PGD=%lx P4D=%lx PUD=%lx PMD=%lx\n",         (unsigned long)caller->mm->pgd,
         (unsigned long)caller->mm->p4d,
         (unsigned long)caller->mm->pud,
         (unsigned long)caller->mm->pmd);
  return 0;
}
#endif  //def MM64