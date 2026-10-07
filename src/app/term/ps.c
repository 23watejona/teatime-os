#include "proc.h"
#include "term.h"
#include "term_command.h"

static const char *const state_names[] = {
    [PROC_UNUSED] = "unused",
    [PROC_UNAVAIL] = "unavail",
    [PROC_AVAIL] = "ready",
    [PROC_CURR] = "running",
    [PROC_COND_WAIT] = "cond wait",
    [PROC_SEM_WAIT] = "sem wait",
    [PROC_TIMED_WAIT] = "timed wait",
};

static void run(struct term *t, int argc, char **argv) {
    for (int pid = 0; pid < NUM_PROC; pid++) {
        unsigned int status = proctab[pid].status;
        if (status == PROC_UNUSED) {
            continue;
        }
        term_put_number(t, pid);
        term_put(t, " ");
        const char *state_name = "?";
        if (status < sizeof(state_names) / sizeof(state_names[0])) {
            state_name = state_names[status];
        }
        term_put(t, state_name);
        term_put(t, " prio ");
        term_put_number(t, proctab[pid].priority);
        term_put(t, TERM_NEWLINE);
    }
}

const struct term_command ps_command = { "ps", "list processes", run };
