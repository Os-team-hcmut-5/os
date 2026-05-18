#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        if (q == NULL || proc == NULL)
                return;

        if (q->size >= MAX_QUEUE_SIZE)
                return;

        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /*
         * Lấy process ở đầu queue (FIFO / index 0).
         *
         * - MLQ_SCHED: priority đã được xử lý ở tầng mlq_ready_queue[]
         *   trong sched.c, nên trong mỗi queue con chỉ cần FIFO.
         * - Plain RR: Round-Robin cũng là FIFO — process nào vào trước
         *   thì được chạy trước, không so sánh priority ở đây.
         *
         * Cả hai mode đều lấy index 0.
         */
        struct pcb_t *proc;
        int i;

        if (empty(q))
                return NULL;

        proc = q->proc[0];
        for (i = 0; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];

        q->size--;
        q->proc[q->size] = NULL;

        return proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        int i;
        int found;

        if (empty(q) || proc == NULL)
                return NULL;

        found = -1;
        for (i = 0; i < q->size; i++) {
                if (q->proc[i] == proc) {
                        found = i;
                        break;
                }
        }

        if (found == -1)
                return NULL;

        proc = q->proc[found];
        for (i = found; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];

        q->size--;
        q->proc[q->size] = NULL;

        return proc;
}