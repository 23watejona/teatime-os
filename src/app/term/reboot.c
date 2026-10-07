#include "proc.h"
#include "wdt.h"
#include "term.h"
#include "term_command.h"

#define REBOOT_FLUSH_TICKS (10)

static void run(struct term *t, int argc, char **argv) {
    term_put(t, "rebooting");
    term_put(t, TERM_NEWLINE);
    term_flush(t);
    // give the transport time to send the flushed output before the reset cuts it off
    sleep(REBOOT_FLUSH_TICKS);
    system_reboot();
}

const struct term_command reboot_command = { "reboot", "restart the board", run };
