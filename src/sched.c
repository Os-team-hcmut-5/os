#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void)
{
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++)
        if (!empty(&mlq_ready_queue[prio]))
            return -1;
#endif
    return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void)
{
#ifdef MLQ_SCHED
    int i;

    for (i = 0; i < MAX_PRIO; i++)
    {
        mlq_ready_queue[i].size = 0;
        slot[i] = MAX_PRIO - i;
    }
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    running_list.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED

// /* Helper: Kiểm tra xem tất cả các queue có process đều đã cạn slot chưa */
// static int all_slots_empty(void) {
//     int i;
//     for (i = 0; i < MAX_PRIO; i++) {
//         if (!empty(&mlq_ready_queue[i]) && slot[i] > 0) {
//             return 0; /* Vẫn còn queue có process và còn slot */
//         }
//     }
//     return 1;
// }

// /* Helper: Reset lại slot cho tất cả các level */
// static void reset_slot(void) {
//     int i;
//     for (i = 0; i < MAX_PRIO; i++) {
//         slot[i] = MAX_PRIO - i;
//     }
// }

/*
 * Stateful design for routine calling
 * based on the priority and our MLQ policy
 * We implement stateful here using transition technique
 * State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
    struct pcb_t * proc = NULL;

    pthread_mutex_lock(&queue_lock);

    uint32_t prio;
    int found = 0;

    // Tìm tiến trình ở hàng đợi CÓ THỂ CHẠY (không rỗng VÀ vẫn còn slot)
    for (prio = 0; prio < MAX_PRIO; ++prio) {
        if (!empty(&mlq_ready_queue[prio]) && slot[prio] > 0) {
            proc = dequeue(&mlq_ready_queue[prio]);
            --slot[prio];
            found = 1;
            break;
        }
    }

    // Nếu không tìm thấy tiến trình nào do TẤT CẢ các hàng đợi đều hết slot,
    // nhưng vẫn CÒN tiến trình đang chờ -> Cấp lại slot cho vòng mới (reset epoch)
    if (!found) {
        int has_process = 0;
        // Kiểm tra xem thực sự còn tiến trình nào không
        for (prio = 0; prio < MAX_PRIO; ++prio) {
            if (!empty(&mlq_ready_queue[prio])) {
                has_process = 1;
                break;
            }
        }

        if (has_process) {
            // Nạp lại slot cho TOÀN BỘ các hàng đợi
            for (prio = 0; prio < MAX_PRIO; ++prio) {
                slot[prio] = MAX_PRIO - prio;
            }
            
            // Lấy ra tiến trình đầu tiên ở hàng đợi cao nhất sau khi đã reset slot
            for (prio = 0; prio < MAX_PRIO; ++prio) {
                if (!empty(&mlq_ready_queue[prio])) {
                    proc = dequeue(&mlq_ready_queue[prio]);
                    --slot[prio];
                    break;
                }
            }
        }
    }

    if (proc != NULL)
        enqueue(&running_list, proc);
        
    pthread_mutex_unlock(&queue_lock); // Đừng quên mở khoá Mutex!
    
    return proc;    
}

void put_mlq_proc(struct pcb_t *proc)
{
    proc->krnl->ready_queue = &ready_queue;
    proc->krnl->mlq_ready_queue = mlq_ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    
    /* Gỡ process khỏi danh sách đang chạy */
    purgequeue(&running_list, proc);
    
    /* Đưa process về cuối hàng đợi tương ứng với mức độ ưu tiên của nó */
    enqueue(&mlq_ready_queue[proc->prio], proc);
    
    pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc)
{
    proc->krnl->ready_queue = &ready_queue;
    proc->krnl->mlq_ready_queue = mlq_ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    
    /* Đưa process mới vào cuối hàng đợi tương ứng với mức độ ưu tiên */
    enqueue(&mlq_ready_queue[proc->prio], proc);
    
    pthread_mutex_unlock(&queue_lock);
}

struct pcb_t *get_proc(void)
{
    return get_mlq_proc();
}

void put_proc(struct pcb_t *proc)
{
    return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc)
{
    return add_mlq_proc(proc);
}

#else

/* Chế độ Non-MLQ (Round-Robin cơ bản) */

struct pcb_t *get_proc(void)
{
    struct pcb_t *proc = NULL;

    pthread_mutex_lock(&queue_lock);
    
    /* Ưu tiên lấy từ ready_queue trước, nếu rỗng thì lấy từ run_queue */
    if (!empty(&ready_queue)) {
        proc = dequeue(&ready_queue);
    }
    else if (!empty(&run_queue)) {
        proc = dequeue(&run_queue);
    }

    /* Đưa vào danh sách đang chạy */
    if (proc != NULL) {
        enqueue(&running_list, proc);
    }

    pthread_mutex_unlock(&queue_lock);

    return proc;
}

void put_proc(struct pcb_t *proc)
{
    proc->krnl->ready_queue = &ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    
    /* Gỡ khỏi danh sách đang chạy */
    purgequeue(&running_list, proc);   
    
    /* Hết quantum nhưng chưa xong, đẩy vào run_queue */
    enqueue(&run_queue, proc);
    
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc)
{
    proc->krnl->ready_queue = &ready_queue;
    proc->krnl->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    
    /* Process mới vào, đẩy vào ready_queue */
    enqueue(&ready_queue, proc);
    
    pthread_mutex_unlock(&queue_lock);
}
#endif