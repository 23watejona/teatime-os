#include "def.h"
#include "dev.h"
#include "string.h"
#include "term.h"
#include "term_command.h"

#define ARGS_MAX (8)
#define READ_CHUNK (32)
#define NUMBER_CHARS (12)

#define KEY_BACKSPACE (0x08)
#define KEY_DELETE (0x7f)
#define KEY_KILL_LINE (0x15)
#define KEY_RETURN '\r'
#define KEY_NEWLINE '\n'
#define KEY_SPACE ' '
#define KEY_LAST_PRINTABLE '~'
#define KEY_ESCAPE (0x1b)
#define KEY_CSI '['
#define KEY_SS3 'O'
#define CSI_FINAL_FIRST (0x40)
#define CSI_FINAL_LAST (0x7e)

static const char prompt[] = "> ";
static const char erase[] = "\b \b";

static const struct term_command *const commands[] = {
    &ps_command,
    &mem_command,
    &ip_command,
    &exit_command,
    &reboot_command,
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

void term_flush(struct term *t) {
    if (t->out_len) {
        write(t->fd, t->out, t->out_len);
    }
    t->out_len = 0;
}

void term_put_char(struct term *t, char c) {
    if (t->out_len == TERM_OUT_MAX) {
        term_flush(t);
    }
    t->out[t->out_len++] = c;
}

void term_put(struct term *t, const char *s) {
    while (*s) {
        term_put_char(t, *s++);
    }
}

void term_put_number(struct term *t, unsigned int n) {
    char buf[NUMBER_CHARS];
    term_put(t, itoau(n, buf, 10));
}

static void list_commands(struct term *t) {
    term_put(t, "help - this list");
    term_put(t, TERM_NEWLINE);
    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        term_put(t, commands[i]->name);
        term_put(t, " - ");
        term_put(t, commands[i]->help);
        term_put(t, TERM_NEWLINE);
    }
}

static char *arg_end(char *arg) {
    while (*arg && *arg != KEY_SPACE) {
        ++arg;
    }

    return arg;
}

// build the argv array, with pointers into the line buffer
static void build_args(char *line, char **argv, int *argc) {
    while (*line && *argc < ARGS_MAX) {
        while (*line == KEY_SPACE) {
            line++;
        }

        if (*line == '\0') {
            break;
        }

        argv[(*argc)++] = line;
        line = arg_end(line);
        if (*line) {
            *line++ = '\0';
        }
    }
}

static void run_line(struct term *t) {
    char *argv[ARGS_MAX];
    int argc = 0;
    t->line[t->line_len] = '\0';
    build_args(t->line, argv, &argc);

    if (argc == 0) {
        return;
    }
    if (strcmp(argv[0], "help") == 0) {
        list_commands(t);
        return;
    }
    for (unsigned int i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i]->name) == 0) {
            commands[i]->run(t, argc, argv);
            return;
        }
    }
    term_put(t, "unknown command: ");
    term_put(t, argv[0]);
    term_put(t, TERM_NEWLINE);
}

void term_mark_done(struct term *t) {
    t->state = TERM_DONE;
}

// all handler functions for specific states assume we're actually in
// the appropriate state when called, and will not verify
static void term_handle_normal(struct term *t, unsigned char c) {
    unsigned int old_line_len = t->line_len;
    int should_echo = t->echo_enabled(t->fd);
    switch (c) {
        case KEY_KILL_LINE:
            t->line_len = 0;
            // fallthrough - shared logic
        case KEY_BACKSPACE:
        case KEY_DELETE:
            // safety against underflow, and prevent double dec after kill
            if (t->line_len > 0) {
                t->line_len--;
            }
            while (old_line_len-- > t->line_len) {
                if (should_echo) {
                    term_put(t, erase);
                }
            }
            break;
        case KEY_RETURN:
            t->state = TERM_LINE_END;
            // fallthrough - shared logic
        case KEY_NEWLINE:
            if (should_echo) {
                term_put(t, TERM_NEWLINE);
            }
            run_line(t);
            t->line_len = 0;
            if (t->state != TERM_DONE) {
                term_put(t, prompt);
            }
            break;
        case KEY_ESCAPE:
            t->state = TERM_ESCAPE;
            break;
        default:
            // only add if it's a printable character
            if (c >= KEY_SPACE && c <= KEY_LAST_PRINTABLE && t->line_len < TERM_LINE_MAX - 1) {
                t->line[t->line_len++] = c;
                if (should_echo) {
                    term_put_char(t, c);
                }
            }
    }
}

static void term_handle_line_end(struct term *t, unsigned char c) {
    // swallow newlines after a CR, everything else should be handled
    // normally
    t->state = TERM_NORMAL;
    if (c != KEY_NEWLINE) {
        term_handle_normal(t, c);
    }
}

static void term_handle_escape(struct term *t, unsigned char c) {
    switch (c) {
        case KEY_CSI:
            t->state = TERM_CSI;
            break;
        case KEY_SS3:
            t->state = TERM_SS3;
            break;
        default:
            t->state = TERM_NORMAL;
            // we are dropping this character
    }
}

static void term_handle_csi(struct term *t, unsigned char c) {
    // for now we swallow these
    if (c >= CSI_FINAL_FIRST && c <= CSI_FINAL_LAST) {
        t->state = TERM_NORMAL;
    }
}

// for now this duplicates the csi handler, keep it separate
// in case these are handled in the future
static void term_handle_ss3(struct term *t, unsigned char c) {
    // for now we swallow these
    if (c >= CSI_FINAL_FIRST && c <= CSI_FINAL_LAST) {
        t->state = TERM_NORMAL;
    }
}

static void term_handle_key(struct term *t, unsigned char c) {
    switch (t->state) {
        case TERM_NORMAL:
            term_handle_normal(t, c);
            break;
        case TERM_LINE_END:
            term_handle_line_end(t, c);
            break;
        case TERM_ESCAPE:
            term_handle_escape(t, c);
            break;
        case TERM_CSI:
            term_handle_csi(t, c);
            break;
        case TERM_SS3:
            term_handle_ss3(t, c);
            break;
        case TERM_DONE:
            // do nothing
            break;
    }
}

static void term_parse_buf(struct term *t, unsigned char *buf, int buf_len) {
    for (int i = 0; i < buf_len; ++i) {
        term_handle_key(t, buf[i]);
    }
    term_flush(t);
}

void term_session(int fd, int (*echo_enabled)(int fd)) {
    struct term t = { .fd = fd, .echo_enabled = echo_enabled };
    unsigned char buf[READ_CHUNK];
    term_put(&t, prompt);
    term_flush(&t);
    while (t.state != TERM_DONE) {
        int n = read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        term_parse_buf(&t, buf, n);
    }
}

