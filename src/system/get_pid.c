#include "proc.h"

int get_pid() {
    for (int i = 1; i < NUM_PROC; ++i) {
        if (proctab[i].status == PROC_UNUSED) {
            proctab[i].status = PROC_UNAVAIL;
            return i;
        }
    }
    return -1;
}
