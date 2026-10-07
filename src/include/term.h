#ifndef TERM_H
#define TERM_H

#define TERM_LINE_MAX 80
#define TERM_OUT_MAX 256
#define TERM_NEWLINE "\r\n"

enum term_state {
    TERM_NORMAL,
    TERM_LINE_END,
    TERM_ESCAPE,
    TERM_CSI,
    TERM_SS3,
    TERM_DONE
};

struct term {
    int fd;
    char line[TERM_LINE_MAX];
    enum term_state state;
    unsigned int line_len;
    char out[TERM_OUT_MAX];
    unsigned int out_len;
    int (*echo_enabled)(int fd);
};

void term_flush(struct term *t);
void term_put_char(struct term *t, char c);
void term_put(struct term *t, const char *s);
void term_put_number(struct term *t, unsigned int n);
void term_mark_done(struct term *t);

void term_session(int fd, int (*echo_enabled)(int fd));

#endif
