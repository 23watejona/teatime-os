#ifndef PROC_QUEUE_H
#define PROC_QUEUE_H
#include "proc.h"

// Each process can be in at most one queue
// and we have 2 extra entries for the head and tail of each list
#define NUM_QENT (NUM_PROC + 2 + 2 * NSEM)
#define MIN_KEY (INT_MIN)
#define MAX_KEY (INT_MAX)


typedef struct queue_entry {
    int key; // ordering of queue
    int pid;
    struct queue_entry *prev; // prev element id in queue table
    struct queue_entry *next; // next element id in queue table
} queue_entry;

extern queue_entry process_queues[NUM_QENT];
extern queue_entry *new_queue();
extern void proc_enqueue(queue_entry *queue, int pid, int key);
extern void proc_remove(int pid);
extern int proc_dequeue(queue_entry *queue);


#endif // PROC_QUEUE_H
