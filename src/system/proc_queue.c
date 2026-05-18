#include "proc_queue.h"
#include "def.h"
#include "uart.h"

// first NPROC entries reference the processes
// all entries afterwards go in pairs as head,tail for a new queue
queue_entry process_queues[NUM_QENT];

// first NUM_PROC entries in the table are for the associated pid
static int curr_queue_id = NUM_PROC;

queue_entry *new_queue() {
    int queue_head_id = curr_queue_id;
    int queue_tail_id = curr_queue_id + 1;
    queue_entry *queue_head = &process_queues[queue_head_id];
    queue_entry *queue_tail = &process_queues[queue_tail_id];

    curr_queue_id += 2;

    // initialize head and tails
    queue_head->prev = NULL;
    queue_head->next = queue_tail;
    queue_head->key = MAX_KEY;  

    queue_tail->prev = queue_head;
    queue_tail->next = NULL;
    queue_tail->key = MIN_KEY;

    return queue_head;
}

IRAM_ATTR void proc_enqueue(queue_entry *queue, int pid, int key) {
    queue_entry *process_queue_entry = &process_queues[pid];
    process_queue_entry->pid = pid;
    process_queue_entry->key = key;
    while(queue->key >= key) {
        if (queue == process_queue_entry) {
            return;
        }
        queue = queue->next;
    }

    process_queue_entry->next = queue->prev->next;
    queue->prev->next = process_queue_entry;
    process_queue_entry->prev = queue->prev;
    queue->prev = process_queue_entry;
}


int proc_dequeue(queue_entry *queue) {
    queue_entry *process_queue_entry = queue->next;
    
    process_queue_entry->next->prev = process_queue_entry->prev;
    process_queue_entry->prev->next = process_queue_entry->next;
    
    process_queue_entry->next = NULL;
    process_queue_entry->prev = NULL;
    
    return process_queue_entry->pid;
}
