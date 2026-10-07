#ifndef TERM_COMMAND_H
#define TERM_COMMAND_H

#include "term.h"

struct term_command {
    const char *name;
    const char *help;
    void (*run)(struct term *t, int argc, char **argv);
};


extern const struct term_command ps_command;
extern const struct term_command mem_command;
extern const struct term_command ip_command;
extern const struct term_command uart_command;
extern const struct term_command exit_command;
extern const struct term_command reboot_command;

#endif
