// /*
//  * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
//  */

// /* LamiaAtrium release
//  * Source Code License Grant: The authors hereby grant to Licensee
//  * personal permission to use and modify the Licensed Source Code
//  * for the sole purpose of studying while attending the course CO2018.
//  */

// #include "queue.h"
// #include "sched.h"
// #include <pthread.h>

// #include <stdlib.h>
// #include <stdio.h>

// static struct queue_t ready_queue;
// static struct queue_t run_queue;
// static pthread_mutex_t queue_lock;

// static struct queue_t running_list;

// #ifdef MLQ_SCHED
// static struct queue_t mlq_ready_queue[MAX_PRIO];
// static int slot[MAX_PRIO];

// /*
//  * MLQ state: tracks which priority level we are currently serving
//  * and how many slots have been consumed at that level.
//  *
//  * Policy recap:
//  *   - Priority 0 is the HIGHEST priority.
//  *   - Each priority level p is given (MAX_PRIO - p) time slots per round
//  *     before the scheduler moves to the next non-empty level.
//  *   - Within one priority level the processes are served FIFO (Round Robin
//  *     among peers): dequeue picks the head, the caller runs it for one
//  *     quantum and then re-enqueues it via put_mlq_proc if it is not done.
//  *   - If the current level becomes empty we drop to the next level
//  *     immediately (no wasted slots).
//  */
// static int  mlq_curr_prio     = 0;   /* priority level being served now   */
// static int  mlq_curr_slot_used = 0;  /* slots consumed at mlq_curr_prio   */
// #endif

// /* ------------------------------------------------------------------ */

// int queue_empty(void) {
// #ifdef MLQ_SCHED
//     unsigned long prio;
//     for (prio = 0; prio < MAX_PRIO; prio++)
//         if (!empty(&mlq_ready_queue[prio]))
//             return -1;   /* -1 → NOT empty (original convention) */
// #endif
//     return (empty(&ready_queue) && empty(&run_queue));
// }

// /* ------------------------------------------------------------------ */

// void init_scheduler(void) {
// #ifdef MLQ_SCHED
//     int i;
//     for (i = 0; i < MAX_PRIO; i++) {
//         mlq_ready_queue[i].size = 0;
//         slot[i] = MAX_PRIO - i;   /* higher prio → more slots per round */
//     }
//     mlq_curr_prio      = 0;
//     mlq_curr_slot_used = 0;
// #endif
//     ready_queue.size  = 0;
//     run_queue.size    = 0;
//     running_list.size = 0;
//     pthread_mutex_init(&queue_lock, NULL);
// }

// /* ================================================================== */
// #ifdef MLQ_SCHED
// /* ================================================================== */

// /*
//  * get_mlq_proc — pick the next process under the MLQ + Round-Robin policy.
//  *
//  * Algorithm
//  * ---------
//  *  1. Starting from mlq_curr_prio, scan downward (lower priority) to find
//  *     the highest-priority non-empty queue.
//  *  2. If we have exhausted the slot budget for mlq_curr_prio, advance to
//  *     the next non-empty level and reset the slot counter.
//  *  3. Dequeue the HEAD of the chosen level (FIFO within a level = RR).
//  *  4. Add the process to running_list so other subsystems can inspect it.
//  *  5. Unlock before returning.
//  */
// struct pcb_t *get_mlq_proc(void) {
//     struct pcb_t *proc = NULL;
//     int prio;

//     pthread_mutex_lock(&queue_lock);

//     /* --- Step 1 & 2: find the active priority level --- */

//     /*
//      * Walk from the current priority downward looking for a non-empty queue.
//      * If the slot budget at the current level is exhausted, we must move on.
//      */
//     for (prio = mlq_curr_prio; prio < MAX_PRIO; prio++) {
//         if (!empty(&mlq_ready_queue[prio])) {
//             /* Found a non-empty queue at this level */
//             if (prio != mlq_curr_prio) {
//                 /* We skipped one or more empty levels — reset state */
//                 mlq_curr_prio      = prio;
//                 mlq_curr_slot_used = 0;
//             }
//             break;
//         }
//     }

//     if (prio >= MAX_PRIO) {
//         /* All queues are empty */
//         pthread_mutex_unlock(&queue_lock);
//         return NULL;
//     }

//     /* --- Check slot budget at the chosen level --- */
//     if (mlq_curr_slot_used >= slot[mlq_curr_prio]) {
//         /*
//          * Budget exhausted. Find the next non-empty level
//          * (wrap around the whole array if needed).
//          */
//         int next = -1;
//         int i;

//         /* Search levels BELOW current first (lower priority) */
//         for (i = mlq_curr_prio + 1; i < MAX_PRIO; i++) {
//             if (!empty(&mlq_ready_queue[i])) {
//                 next = i;
//                 break;
//             }
//         }
//         /* If nothing below, wrap to the top */
//         if (next == -1) {
//             for (i = 0; i <= mlq_curr_prio; i++) {
//                 if (!empty(&mlq_ready_queue[i])) {
//                     next = i;
//                     break;
//                 }
//             }
//         }

//         if (next == -1) {
//             /* Still nothing — all queues empty */
//             pthread_mutex_unlock(&queue_lock);
//             return NULL;
//         }

//         mlq_curr_prio      = next;
//         mlq_curr_slot_used = 0;
//     }

//     /* --- Step 3: dequeue from the chosen level (FIFO = RR within level) --- */
//     proc = dequeue(&mlq_ready_queue[mlq_curr_prio]);

//     if (proc != NULL) {
//         mlq_curr_slot_used++;          /* consume one slot          */
//         enqueue(&running_list, proc);  /* Step 4: track as running  */
//     }

//     pthread_mutex_unlock(&queue_lock); /* Step 5 */
//     return proc;
// }

// /* ------------------------------------------------------------------ */

// /*
//  * put_mlq_proc — called when the running process yields / its quantum expires
//  *               but it is NOT finished.  It goes BACK into the ready queue at
//  *               its own priority level (Round-Robin re-insertion).
//  *
//  * We also remove it from running_list since it is no longer running.
//  */
// void put_mlq_proc(struct pcb_t *proc) {
//     proc->krnl->ready_queue      = &ready_queue;
//     proc->krnl->mlq_ready_queue  = mlq_ready_queue;
//     proc->krnl->running_list     = &running_list;

//     pthread_mutex_lock(&queue_lock);

//     /* Remove from running_list — it is no longer executing */
//     purgequeue(&running_list, proc);

//     /* Re-insert at the TAIL of its priority queue (Round Robin) */
//     enqueue(&mlq_ready_queue[proc->prio], proc);

//     pthread_mutex_unlock(&queue_lock);
// }

// /* ------------------------------------------------------------------ */

// /*
//  * add_mlq_proc — called when a BRAND-NEW process is admitted to the system.
//  *               It enters the ready queue at its assigned priority level.
//  */
// void add_mlq_proc(struct pcb_t *proc) {
//     proc->krnl->ready_queue     = &ready_queue;
//     proc->krnl->mlq_ready_queue = mlq_ready_queue;
//     proc->krnl->running_list    = &running_list;

//     pthread_mutex_lock(&queue_lock);
//     enqueue(&mlq_ready_queue[proc->prio], proc);
//     pthread_mutex_unlock(&queue_lock);
// }

// /* ------------------------------------------------------------------ */

// struct pcb_t *get_proc(void)          { return get_mlq_proc();  }
// void          put_proc(struct pcb_t *proc) { put_mlq_proc(proc); }
// void          add_proc(struct pcb_t *proc) { add_mlq_proc(proc); }

// /* ================================================================== */
// #else   /* plain Round-Robin scheduler (no MLQ) */
// /* ================================================================== */

// /*
//  * get_proc — dequeue the next process from the ready queue.
//  *
//  * Simple RR: processes are served FIFO from ready_queue.
//  * dequeue() already picks index 0 (the head) when MLQ_SCHED is off,
//  * so no extra ordering logic is needed here.
//  */
// struct pcb_t *get_proc(void) {
//     struct pcb_t *proc = NULL;

//     pthread_mutex_lock(&queue_lock);

//     /* Try the ready_queue first; fall back to run_queue */
//     if (!empty(&ready_queue)) {
//         proc = dequeue(&ready_queue);
//     } else if (!empty(&run_queue)) {
//         proc = dequeue(&run_queue);
//     }

//     if (proc != NULL)
//         enqueue(&running_list, proc);   /* mark as running */

//     pthread_mutex_unlock(&queue_lock);
//     return proc;
// }

// /* ------------------------------------------------------------------ */

// /*
//  * put_proc — running process yields (quantum expired, not finished).
//  *            Remove from running_list, place at the tail of run_queue
//  *            so it will be re-promoted to ready_queue next round (RR).
//  */
// void put_proc(struct pcb_t *proc) {
//     proc->krnl->ready_queue  = &ready_queue;
//     proc->krnl->running_list = &running_list;

//     pthread_mutex_lock(&queue_lock);

//     purgequeue(&running_list, proc);   /* no longer running */
//     enqueue(&run_queue, proc);         /* back to run queue  */

//     pthread_mutex_unlock(&queue_lock);
// }

// /* ------------------------------------------------------------------ */

// /*
//  * add_proc — admit a new process into the scheduler.
//  *            New processes always enter ready_queue.
//  */
// void add_proc(struct pcb_t *proc) {
//     proc->krnl->ready_queue  = &ready_queue;
//     proc->krnl->running_list = &running_list;

//     pthread_mutex_lock(&queue_lock);
//     enqueue(&ready_queue, proc);
//     pthread_mutex_unlock(&queue_lock);
// }

// #endif /* MLQ_SCHED */

#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>

#define MAX_PRIO 3

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif


// #include "queue.h"
// #include "sched.h"
// #include <pthread.h>

// #include <stdlib.h>
// #include <stdio.h>

// static struct queue_t ready_queue;
// static struct queue_t run_queue;
// static pthread_mutex_t queue_lock;

// static struct queue_t running_list;

// #ifdef MLQ_SCHED
// static struct queue_t mlq_ready_queue[MAX_PRIO];

// /*
//  * slot[i] — ngân sách còn lại của queue i trong round hiện tại.
//  *
//  * Khởi tạo và reset: slot[i] = MAX_PRIO - i
//  * Mỗi lần dequeue từ queue i:  slot[i]--
//  * Khi slot[i] == 0: queue i đã hết lượt, không được dequeue thêm.
//  * Khi TẤT CẢ queue hết slot (hoặc rỗng): reset slot[i] = MAX_PRIO - i
//  * và bắt đầu round mới.
//  *
//  * Ví dụ MAX_PRIO=3:
//  *   Round 1:  prio0 chạy 3 lần, prio1 chạy 2 lần, prio2 chạy 1 lần
//  *   → reset → Round 2: lặp lại
//  */
// static int slot[MAX_PRIO];
// #endif

/* ------------------------------------------------------------------ */

int queue_empty(void) {
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++)
        if (!empty(&mlq_ready_queue[prio]))
            return -1;   /* -1 → NOT empty */
    return 1;
#endif
    return (empty(&ready_queue) && empty(&run_queue));
}

/* ------------------------------------------------------------------ */

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i;
    for (i = 0; i < MAX_PRIO; i++) {
        mlq_ready_queue[i].size = 0;
        slot[i] = MAX_PRIO - i;   /* prio cao hơn → slot nhiều hơn */
    }
#endif
    ready_queue.size  = 0;
    run_queue.size    = 0;
    running_list.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

/* ================================================================== */
#ifdef MLQ_SCHED
/* ================================================================== */

/*
 * reset_slot — khôi phục toàn bộ ngân sách slot về giá trị ban đầu.
 * Gọi khi tất cả queue đã hết slot (kết thúc một round).
 */
static void reset_slot(void) {
    int i;
    for (i = 0; i < MAX_PRIO; i++)
        slot[i] = MAX_PRIO - i;
}

/*
 * all_slots_empty — kiểm tra xem tất cả queue có process đều đã hết slot chưa.
 * Trả về 1 nếu mọi queue không rỗng đều có slot[i] == 0 (hết lượt).
 */
static int all_slots_empty(void) {
    int i;
    for (i = 0; i < MAX_PRIO; i++)
        if (!empty(&mlq_ready_queue[i]) && slot[i] > 0)
            return 0;   /* còn ít nhất 1 queue có process VÀ còn slot */
    return 1;
}

/*
 * get_mlq_proc — chọn process tiếp theo theo chính sách MLQ + Round-Robin.
 *
 * Thuật toán (đúng theo spec):
 * ------------------------------------------------------------
 * Bước 1: Quét từ prio 0 → MAX_PRIO-1
 *         Tìm queue đầu tiên THỎA MÃN: không rỗng VÀ slot[i] > 0
 *
 * Bước 2: Nếu không tìm được (tất cả queue rỗng hoặc hết slot):
 *         → Kiểm tra có queue nào còn process không
 *           - Không có:  return NULL (hệ thống rỗng)
 *           - Có nhưng hết slot: reset_slot() rồi quét lại từ Bước 1
 *
 * Bước 3: dequeue từ queue[i] (FIFO trong một level = Round Robin)
 *         slot[i]--
 *
 * Bước 4: Thêm vào running_list, unlock, return proc.
 */
struct pcb_t *get_mlq_proc(void) {
    struct pcb_t *proc = NULL;
    int i;
    int selected;

    pthread_mutex_lock(&queue_lock);

    /* ---- Bước 1 & 2: tìm queue hợp lệ, reset nếu cần ---- */
retry:
    selected = -1;
    for (i = 0; i < MAX_PRIO; i++) {
        if (!empty(&mlq_ready_queue[i]) && slot[i] > 0) {
            selected = i;
            break;   /* lấy level ưu tiên CAO NHẤT còn slot */
        }
    }

    if (selected == -1) {
        /* Không tìm được queue hợp lệ — tại sao? */
        if (all_slots_empty()) {
            /*
             * Tất cả queue có process đều hết slot → kết thúc round.
             * Reset và bắt đầu round mới.
             */
            if (queue_empty() == 1) {
                /* Thực sự không còn process nào */
                pthread_mutex_unlock(&queue_lock);
                return NULL;
            }
            reset_slot();
            goto retry;   /* quét lại sau khi reset */
        } else {
            /* Không có process nào trong hệ thống */
            pthread_mutex_unlock(&queue_lock);
            return NULL;
        }
    }

    /* ---- Bước 3: dequeue và trừ slot ---- */
    proc = dequeue(&mlq_ready_queue[selected]);

    if (proc != NULL) {
        slot[selected]--;              /* trừ slot theo đúng spec: slot[i]-- */
        enqueue(&running_list, proc);  /* đánh dấu đang chạy                 */
    }

    /* ---- Bước 4 ---- */
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

/* ------------------------------------------------------------------ */

/*
 * put_mlq_proc — process hết quantum nhưng CHƯA xong.
 *               Đưa về CUỐI queue của đúng priority level (Round-Robin).
 */
void put_mlq_proc(struct pcb_t *proc) {
    proc->krnl->ready_queue      = &ready_queue;
    proc->krnl->mlq_ready_queue  = mlq_ready_queue;
    proc->krnl->running_list     = &running_list;

    pthread_mutex_lock(&queue_lock);
    purgequeue(&running_list, proc);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

/* ------------------------------------------------------------------ */

/*
 * add_mlq_proc — process MỚI vào hệ thống.
 *               Xếp vào queue theo đúng priority level được gán.
 */
void add_mlq_proc(struct pcb_t *proc) {
    proc->krnl->ready_queue     = &ready_queue;
    proc->krnl->mlq_ready_queue = mlq_ready_queue;
    proc->krnl->running_list    = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&mlq_ready_queue[proc->prio], proc);
    pthread_mutex_unlock(&queue_lock);
}

/* ------------------------------------------------------------------ */

struct pcb_t *get_proc(void)               { return get_mlq_proc();  }
void          put_proc(struct pcb_t *proc) { put_mlq_proc(proc);      }
void          add_proc(struct pcb_t *proc) { add_mlq_proc(proc);      }

/* ================================================================== */
#else   /* Plain Round-Robin (không có MLQ) */
/* ================================================================== */

struct pcb_t *get_proc(void) {
    struct pcb_t *proc = NULL;

    pthread_mutex_lock(&queue_lock);

    if (!empty(&ready_queue))
        proc = dequeue(&ready_queue);
    else if (!empty(&run_queue))
        proc = dequeue(&run_queue);

    if (proc != NULL)
        enqueue(&running_list, proc);

    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t *proc) {
    proc->krnl->ready_queue  = &ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    purgequeue(&running_list, proc);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc) {
    proc->krnl->ready_queue  = &ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

#endif /* MLQ_SCHED */