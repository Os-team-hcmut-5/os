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
<<<<<<< HEAD
        if (q == NULL || proc == NULL)
                return;

        if (q->size >= MAX_QUEUE_SIZE)
                return;

        q->proc[q->size] = proc;
        q->size++;
=======
        /* TODO: put a new process to queue [q] */
;
>>>>>>> af7380308d78b23da71527644ea92154808d7398
}

struct pcb_t *dequeue(struct queue_t *q)
{
<<<<<<< HEAD
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
=======
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */

		return NULL;
>>>>>>> af7380308d78b23da71527644ea92154808d7398
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
<<<<<<< HEAD
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
=======
        /* TODO: remove a specific item from queue
         * */
        return NULL;
>>>>>>> af7380308d78b23da71527644ea92154808d7398
}