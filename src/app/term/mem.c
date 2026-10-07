#include "mem.h"
#include "term.h"
#include "term_command.h"

static void run(struct term *t, int argc, char **argv) {
    term_put(t, "heap ");
    term_put_number(t, (unsigned int)heap_end - (unsigned int)heap_start);
    term_put(t, " free ");
    term_put_number(t, heap_available());
    term_put(t, TERM_NEWLINE);
}

const struct term_command mem_command = { "mem", "heap size and free bytes", run };
