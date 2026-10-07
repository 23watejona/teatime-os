#include "term.h"
#include "term_command.h"

static void run(struct term *t, int argc, char **argv) {
    term_mark_done(t);
}

const struct term_command exit_command = { "exit", "end this session", run };
