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

	/* TODO: implement the page direactories mapping */

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
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
  // pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  //pte = &krnl->mm->pt;
  // 1. Cấp 5: PGD (Page Global Directory)
  if (krnl->mm->pgd == NULL) {
      krnl->mm->pgd = calloc(512, sizeof(addr_t));
  }
  // 2. Cấp 4: P4D
  addr_t *p4d_tbl = (addr_t *)krnl->mm->pgd[pgd];
  if (p4d_tbl == NULL) {
      p4d_tbl = calloc(512, sizeof(addr_t));
      krnl->mm->pgd[pgd] = (addr_t)p4d_tbl;
  }
  // 3. Cấp 3: PUD (Page Upper Directory)
  addr_t *pud_tbl = (addr_t *)p4d_tbl[p4d];
  if (pud_tbl == NULL) {
      pud_tbl = calloc(512, sizeof(addr_t));
      p4d_tbl[p4d] = (addr_t)pud_tbl;
  }
  // 4. Cấp 2: PMD (Page Middle Directory)
  addr_t *pmd_tbl = (addr_t *)pud_tbl[pud];
  if (pmd_tbl == NULL) {
      pmd_tbl = calloc(512, sizeof(addr_t));
      pud_tbl[pud] = (addr_t)pmd_tbl;
  }
  // 5. Cấp 1: PT (Page Table)
  addr_t *pt_tbl = (addr_t *)pmd_tbl[pmd];
  if (pt_tbl == NULL) {
      pt_tbl = calloc(512, sizeof(addr_t));
      pmd_tbl[pmd] = (addr_t)pt_tbl;
  }
  // Trỏ PTE vào đúng ô trong Page Table cuối cùng
  pte = &pt_tbl[pt];
#else
  pte = &krnl->mm->pgd[pgn];
#endif
	
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
struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  /* dummy pte alloc to avoid runtime error - Removed to avoid memory leak */
  // pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  //pte = &krnl->mm->pt;
  if (krnl->mm->pgd == NULL) {
      krnl->mm->pgd = calloc(512, sizeof(addr_t));
  }
  
  addr_t *p4d_tbl = (addr_t *)krnl->mm->pgd[pgd];
  if (p4d_tbl == NULL) {
      p4d_tbl = calloc(512, sizeof(addr_t));
      krnl->mm->pgd[pgd] = (addr_t)p4d_tbl;
  }

  addr_t *pud_tbl = (addr_t *)p4d_tbl[p4d];
  if (pud_tbl == NULL) {
      pud_tbl = calloc(512, sizeof(addr_t));
      p4d_tbl[p4d] = (addr_t)pud_tbl;
  }

  addr_t *pmd_tbl = (addr_t *)pud_tbl[pud];
  if (pmd_tbl == NULL) {
      pmd_tbl = calloc(512, sizeof(addr_t));
      pud_tbl[pud] = (addr_t)pmd_tbl;
  }

  addr_t *pt_tbl = (addr_t *)pmd_tbl[pmd];
  if (pt_tbl == NULL) {
      pt_tbl = calloc(512, sizeof(addr_t));
      pmd_tbl[pmd] = (addr_t)pt_tbl;
  }

  // Trỏ PTE vào đúng ô trong Page Table cuối cùng
  pte = &pt_tbl[pt];
#else
  pte = &krnl->mm->pgd[pgn];
#endif

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
struct krnl_t *krnl = caller->krnl;
  uint32_t pte = 0;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t	pt=0;
#ifdef MM64
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //... krnl->mm->pgd
  //... krnl->mm->pt
  //pte = &krnl->mm->pt;	
/* Duyệt cây thư mục từ trên xuống dưới */
  if (krnl->mm->pgd == NULL) return 0;
  
  addr_t *p4d_tbl = (addr_t *)krnl->mm->pgd[pgd];
  if (p4d_tbl == NULL) return 0;

  addr_t *pud_tbl = (addr_t *)p4d_tbl[p4d];
  if (pud_tbl == NULL) return 0;

  addr_t *pmd_tbl = (addr_t *)pud_tbl[pud];
  if (pmd_tbl == NULL) return 0;

  addr_t *pt_tbl = (addr_t *)pmd_tbl[pmd];
  if (pt_tbl == NULL) return 0;

  /* Nếu đi đến được tận cùng, trả về giá trị PTE tại đó */
  return (uint32_t)pt_tbl[pt];
#else
  return krnl->mm->pgd[pgn];
#endif
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{struct krnl_t *krnl = caller->krnl;
  addr_t pgd = 0, p4d = 0, pud = 0, pmd = 0, pt = 0;

#ifdef MM64 
  /* Trích xuất 5 index từ page number */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

  /* Cấp phát theo yêu cầu (Demand Allocation) nếu nhánh chưa tồn tại */
  if (krnl->mm->pgd == NULL) {
      krnl->mm->pgd = calloc(512, sizeof(addr_t));
  }
  
  addr_t *p4d_tbl = (addr_t *)krnl->mm->pgd[pgd];
  if (p4d_tbl == NULL) {
      p4d_tbl = calloc(512, sizeof(addr_t));
      krnl->mm->pgd[pgd] = (addr_t)p4d_tbl;
  }

  addr_t *pud_tbl = (addr_t *)p4d_tbl[p4d];
  if (pud_tbl == NULL) {
      pud_tbl = calloc(512, sizeof(addr_t));
      p4d_tbl[p4d] = (addr_t)pud_tbl;
  }

  addr_t *pmd_tbl = (addr_t *)pud_tbl[pud];
  if (pmd_tbl == NULL) {
      pmd_tbl = calloc(512, sizeof(addr_t));
      pud_tbl[pud] = (addr_t)pmd_tbl;
  }

  addr_t *pt_tbl = (addr_t *)pmd_tbl[pmd];
  if (pt_tbl == NULL) {
      pt_tbl = calloc(512, sizeof(addr_t));
      pmd_tbl[pmd] = (addr_t)pt_tbl;
  }

  /* Ghi giá trị vào Page Table cuối cùng */
  pt_tbl[pt] = pte_val;
#else
  krnl->mm->pgd[pgn] = pte_val;
#endif
  
  return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  //int pgit = 0;
  //uint64_t pattern = 0xdeadbeef;

  /* TODO memset the page table with given pattern
   */

  int pgit = 0;
  uint32_t pattern = 0xdeadbeef;

  /* Tính toán số hiệu trang (Page Number) bắt đầu từ địa chỉ ảo addr */
  /* Do addr đã được căn lề (aligned) với kích thước trang, chia cho PAGING_PAGESZ là an toàn */
  addr_t start_pgn = addr / PAGING_PAGESZ; 

  /* Lặp qua từng trang để thực hiện dummy allocation */
  for (pgit = 0; pgit < pgnum; pgit++) {
      addr_t pgn = start_pgn + pgit;
      
      /* Gọi pte_set_entry để ép hệ thống tự động sinh các nhánh cây thư mục 
       * (Demand Allocation) và ghi giá trị pattern (0xdeadbeef) vào ô PTE cuối cùng.
       * Quá trình này KHÔNG hề lấy đi bất kỳ frame nào từ RAM vật lý. */
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
struct framephy_struct *fpit;
int pgit = 0;
addr_t pgn;

  /* TODO: update the rg_end and rg_start of ret_rg 
  //ret_rg->rg_end =  ....
  //ret_rg->rg_start = ...
  //ret_rg->vmaid = ...
  */

  /* TODO map range of frame to address space
   *      [addr to addr + pgnum*PAGING_PAGESZ
   *      in page table caller->krnl->mm->pgd,
   *                    caller->krnl->mm->pud...
   *                    ...
   */

  /* Tracking for later page replacement activities (if needed)
   * Enqueue new usage page */
  //enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn64 + pgit);

 /* Cập nhật ranh giới vùng nhớ trả về */
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
  ret_rg->rg_next = NULL;

  fpit = frames;

  /* Ánh xạ từng trang ảo vào khung trang vật lý */
  for (pgit = 0; pgit < pgnum; pgit++) 
  {
    if (fpit == NULL) {
        break; /* An toàn: Dừng nếu danh sách frame cạn kiệt trước khi map xong */
    }

    pgn = (addr / PAGING_PAGESZ) + pgit;
    
    /* Sử dụng hàm pte_set_fpn đã hoàn thiện để tự động cấp phát thư mục và gán frame */
    pte_set_fpn(caller, pgn, fpit->fpn);

    /* Hàng đợi phục vụ cho thuật toán Page Replacement (FIFO) */
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);

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
  //addr_t fpn;
  //int pgit;
  //struct framephy_struct *newfp_str = NULL;

  /* TODO: allocate the page 
  //caller-> ...
  //frm_lst-> ...
  */


/*
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    // TODO: allocate the page 
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0)
    {
      newfp_str->fpn = fpn;
    }
    else
    { // TODO: ERROR CODE of obtaining somes but not enough frames
    }
  }
*/


  /* End TODO */
addr_t fpn;
  int pgit;
  struct framephy_struct *newfp_str = NULL;
  struct framephy_struct *head = NULL;
  struct framephy_struct *tail = NULL;

  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    /* Lấy frame trống từ RAM thông qua mram của krnl */
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
      newfp_str = malloc(sizeof(struct framephy_struct));
      newfp_str->fpn = fpn;
      newfp_str->fp_next = NULL;

      /* Nối node vào danh sách liên kết */
      if (head == NULL) {
        head = newfp_str;
        tail = newfp_str;
      } else {
        tail->fp_next = newfp_str;
        tail = newfp_str;
      }
    }
    else
    { 
      /* Lỗi không đủ khung trang (Out of Memory) -> Thực hiện Rollback */
      while (head != NULL) {
         struct framephy_struct *temp = head;
         head = head->fp_next;
         /* Trả lại frame đã lấy vào RAM */
         MEMPHY_put_freefp(caller->krnl->mram, temp->fpn);
         free(temp);
      }
      *frm_lst = NULL;
      return -1; 
    }
  }

  *frm_lst = head;
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
  {
    return -1;
  }

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
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
if (vma0 == NULL) return -1;
#ifdef MM64
  mm->pgd = NULL; 
  mm->p4d = NULL;
  mm->pud = NULL;
  mm->pmd = NULL;
  mm->pt  = NULL;
#else
  /* Dành cho kiến trúc 32-bit cũ nếu cần tương thích ngược */
  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
#endif

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  vma0->vm_freerg_list = NULL;
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  // vma0->next = ...

  /* Point vma owner backward */
  //vma0->vm_mm = mm; 

/* Vì đây là VMA đầu tiên và duy nhất lúc này, next của nó là NULL */
  vma0->vm_next = NULL;

  /* Point vma owner backward: Trỏ ngược về cấu trúc mm cha */
  vma0->vm_mm = mm; 

  /* BƯỚC 3: Cập nhật mmap của cấu trúc mm */
  /* Gắn vma0 làm node đầu tiên của danh sách các vùng nhớ ảo (mmap) */
  mm->mmap = vma0;
  // mm->symrgtbl = NULL;
  mm->kcpooltbl = NULL;
  /* Khởi tạo hàng đợi FIFO cho thuật toán Page Replacement (nếu có dùng) */
  mm->fifo_pgn = NULL;

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
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  addr_t pgn_start = start / PAGING_PAGESZ;
  addr_t pgn_end = end / PAGING_PAGESZ;
  addr_t pgit;

  printf("\n============= PAGE TABLE DUMP =============\n");
  printf("Range: [0x%08lx] to [0x%08lx]\n", (unsigned long)start, (unsigned long)end);

  int has_mapped = 0;

  /* Lặp qua từng số hiệu trang (Page Number) trong vùng cần in */
  for (pgit = pgn_start; pgit <= pgn_end; pgit++)
  {
    /* Tận dụng hàm pte_get_entry để tra cứu PTE cực nhanh */
    uint32_t pte = pte_get_entry(caller, pgit);

    /* Chỉ in ra những trang thực sự có dữ liệu (PTE khác 0) */
    if (pte != 0) {
      has_mapped = 1;
      addr_t pgd = 0, p4d = 0, pud = 0, pmd = 0, pt = 0;

      /* Lấy các chỉ số thư mục (index) của trang hiện tại để in log */
      get_pd_from_pagenum(pgit, &pgd, &p4d, &pud, &pmd, &pt);

      /* In cấu trúc 5 cấp thư mục */
      printf(" - PGN [%05lu] | PGD:%03lu P4D:%03lu PUD:%03lu PMD:%03lu PT:%03lu | ", 
             (unsigned long)pgit, (unsigned long)pgd, (unsigned long)p4d, 
             (unsigned long)pud, (unsigned long)pmd, (unsigned long)pt);

      /* Phân tích giá trị PTE */
      if (pte == 0xdeadbeef) {
        printf("PTE: 0xdeadbeef (Dummy/VMA Allocated)\n");
      } else {
        /* In ra giá trị PTE dạng Hex để biết FPN hoặc thông tin Swap */
        printf("PTE: 0x%08x (RAM/SWAP Mapped)\n", pte);
      }
    }
  }

  if (!has_mapped) {
    printf(" - No valid page entries found in this range (All empty).\n");
  }
  printf("===========================================\n\n");

  return 0;
}
#endif  //def MM64
