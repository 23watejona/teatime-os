#include "def.h"
#include "dev.h"
#include "string.h"
#include "telnet.h"

#define TELNET_SE 240
#define TELNET_SB 250
#define TELNET_WILL 251
#define TELNET_WONT 252
#define TELNET_DO 253
#define TELNET_DONT 254
#define TELNET_IAC 255

enum telnet_state {
    TELNET_DATA,
    TELNET_COMMAND,
    TELNET_OPTION,
    TELNET_SUBNEGOTIATION,
    TELNET_SUBNEGOTIATION_COMMAND,
};

enum option_state {
    TELNET_OPTION_STATE_NO,
    TELNET_OPTION_STATE_YES,
    TELNET_OPTION_STATE_WANT_YES,
    TELNET_OPTION_STATE_WANT_NO,
    TELNET_OPTION_UNKNOWN // failure state - couldn't find the option
};

enum telnet_side {
    TELNET_REMOTE,
    TELNET_LOCAL
};

static struct {
    int fd;
    enum telnet_state state;
    unsigned char command;
    struct telnet_options {
        char echo;
        char suppress_go_ahead;
    } local_options;
    struct telnet_options remote_options;
} telnet;

static int offered(unsigned char option) {
    return option == TELNET_OPTION_ECHO || option == TELNET_OPTION_SUPPRESS_GO_AHEAD;
}

static int accepted(unsigned char option) {
    return option == TELNET_OPTION_SUPPRESS_GO_AHEAD;
}

// choose the appropriate side of the transaction to update based on the requestor
// and the command
static enum telnet_side get_option_side(enum telnet_side requestor, char command) {
    enum telnet_side side = TELNET_LOCAL;
    if ((requestor == TELNET_REMOTE && (command == TELNET_WILL || command == TELNET_WONT)) ||
        (requestor == TELNET_LOCAL && (command == TELNET_DO || command == TELNET_DONT))) {
        side = TELNET_REMOTE;
    }
    return side;
}

static struct telnet_options *get_option_table(enum telnet_side side) {
    if (side == TELNET_LOCAL) {
        return &(telnet.local_options);
    } else {
        return &(telnet.remote_options);
    }
}

static char *get_global_option(enum telnet_side side, char option) {
    struct telnet_options *opt_tab = get_option_table(side);
    switch (option) {
        case TELNET_OPTION_ECHO:
            return &opt_tab->echo;
        case TELNET_OPTION_SUPPRESS_GO_AHEAD:
            return &opt_tab->suppress_go_ahead;
        default:
            return NULL;
    }
}

static char get_option_state(enum telnet_side side, char option) {
    char *global_option  = get_global_option(side, option);
    if (global_option == NULL) {
        return TELNET_OPTION_UNKNOWN;
    }
    return *global_option;
}

static int set_option_state(enum telnet_side side, char option, enum option_state option_state) {
    char *global_option = get_global_option(side, option);
    if (global_option == NULL) {
        return TELNET_OPTION_UNKNOWN;
    }

    *global_option = option_state;

    return *global_option;
}

static int option_enabled(enum telnet_side side, char option) {
    int option_state = get_option_state(side, option);

    return option_state == TELNET_OPTION_STATE_YES || option_state == TELNET_OPTION_STATE_WANT_NO;
}

// side is the side of the transaction you wish to enable
// the option on
// this function writes to the wire, and is not guaranteed
// to succeed.
static void enable_option(int fd, enum telnet_side side, char option) {
    if (get_option_state(side, option) != TELNET_OPTION_STATE_NO) {
        return;
    }

    char command = side == TELNET_LOCAL ? TELNET_WILL : TELNET_DO;

    set_option_state(side, option, TELNET_OPTION_STATE_WANT_YES);
    char buf[] = {TELNET_IAC, command, option};

    write(fd, buf, sizeof(buf));
}

static int process_data(unsigned char c) {
    if (c == TELNET_IAC) {
        telnet.state = TELNET_COMMAND;
        return -1;
    }

    return c;
}

static int process_command(unsigned char c) {
    if (c == TELNET_IAC) {
        telnet.state = TELNET_DATA;
        return c;
    }

    if (c >= TELNET_WILL) {
        telnet.command = c;
        telnet.state = TELNET_OPTION;
    } else if (c == TELNET_SB) {
        telnet.state = TELNET_SUBNEGOTIATION;
    } else {
        telnet.state = TELNET_DATA;
    }
    return -1;
}

static int process_option(unsigned char option, char *response, int *response_len) {
    telnet.state = TELNET_DATA;
    if (telnet.command != TELNET_DO && telnet.command != TELNET_WILL &&
         telnet.command != TELNET_DONT && telnet.command != TELNET_WONT) {
        return -1;
    }

    enum telnet_side option_side = get_option_side(TELNET_REMOTE, telnet.command);
    char old_option_state = get_option_state(option_side, option);
    char command = telnet.command;
    char reply;

    if (command == TELNET_DONT || command == TELNET_WONT) {
        set_option_state(option_side, option, TELNET_OPTION_STATE_NO);
        if (old_option_state != TELNET_OPTION_STATE_YES) {
            return -1;
        }
        reply = command == TELNET_DONT ? TELNET_WONT : TELNET_DONT;
    }

    if (command == TELNET_DO) {
        // responses to a previous request from us, we shouldn't reply
        if (old_option_state == TELNET_OPTION_STATE_WANT_NO) {
            set_option_state(option_side, option, TELNET_OPTION_STATE_NO);
            return -1;
        } else if (old_option_state == TELNET_OPTION_STATE_WANT_YES || old_option_state == TELNET_OPTION_STATE_YES) {
            set_option_state(option_side, option, TELNET_OPTION_STATE_YES);
            return -1;
        }

        // a new request to handle, we need to reply
        if (offered(option)) {
            reply = TELNET_WILL;
            set_option_state(option_side, option, TELNET_OPTION_STATE_YES);
        } else {
            reply = TELNET_WONT;
        }
    }

    if (command == TELNET_WILL) {
        // responses to a previous request from us, we shouldn't reply
        if (old_option_state == TELNET_OPTION_STATE_WANT_NO) {
            set_option_state(option_side, option, TELNET_OPTION_STATE_NO);
            return -1;
        } else if (old_option_state == TELNET_OPTION_STATE_WANT_YES || old_option_state == TELNET_OPTION_STATE_YES) {
            set_option_state(option_side, option, TELNET_OPTION_STATE_YES);
            return -1;
        }

        // valid new offer, we should respond
        if (accepted(option)) {
            reply = TELNET_DO;
            set_option_state(option_side, option, TELNET_OPTION_STATE_YES);
        } else {
            reply = TELNET_DONT;
        }
    }

    response[0] = TELNET_IAC;
    response[1] = reply;
    response[2] = option;
    *response_len = 3;

    return -1;
}

static int process_subnegotiation(char c) {
    if (c == TELNET_IAC) {
        telnet.state = TELNET_SUBNEGOTIATION_COMMAND;
    }
    return -1;
}

static int process_subnegotiation_command(char c) {
    telnet.state = c == TELNET_SE ? TELNET_DATA : TELNET_SUBNEGOTIATION;
    return -1;
}

static int process(int fd, unsigned char c) {
    char response[3];
    int response_len = 0;
    int ret;
    switch (telnet.state) {
        case TELNET_DATA:
            ret = process_data(c);
            break;
        case TELNET_COMMAND:
            ret = process_command(c);
            break;
        case TELNET_OPTION:
            // we bubble the response up to here so the sub-handler
            // doesn't have to worry about actually writing
            ret = process_option(c, response, &response_len);
            break;
        case TELNET_SUBNEGOTIATION:
            ret = process_subnegotiation(c);
            break;
        case TELNET_SUBNEGOTIATION_COMMAND:
            ret = process_subnegotiation_command(c);
            break;
        default:
            ret = -1;
    }

    if (response_len > 0) {
        write(fd, response, response_len);
    }
    return ret;
}

static int telnet_open(struct dev *d, int conn) {
    memset(&telnet, 0, sizeof(telnet));
    telnet.fd = conn;
    enable_option(conn, TELNET_LOCAL, TELNET_OPTION_ECHO);
    enable_option(conn, TELNET_LOCAL, TELNET_OPTION_SUPPRESS_GO_AHEAD);
    return 0;
}

static int telnet_close(struct dev *d) {
    return close(telnet.fd);
}

static int telnet_write(struct dev *d, const void *buf, unsigned int n) {
    const char *p = buf;
    int write_start = 0;
    for (int i = 0; i < n; ++i) {
        if (p[i] == TELNET_IAC) {
            char repeat[] = {TELNET_IAC};
            int n_to_write = (i - write_start) + 1;
            int wrote = write(telnet.fd, p + write_start, n_to_write);
            if (wrote <= 0) {
                return -1;
            }

            if (write(telnet.fd, repeat, 1) <= 0) {
                return -1;
            }
            write_start = i + 1;
        }
    }

    if (n - write_start > 0) {
        if (write(telnet.fd, p + write_start, n - write_start) <= 0) {
            return -1;
        }
    }
    return n;
}

static int telnet_read(struct dev *d, void *buf, unsigned int n) {
    char *p = buf;
    int out_bytes = 0;
    while (1) {
        int in_bytes = read(telnet.fd, p, n);
        if (in_bytes <= 0) {
            return in_bytes;
        }

        out_bytes = 0;
        for (int i = 0; i < in_bytes; i++) {
            int c = process(telnet.fd, p[i]);
            if (c >= 0) {
                p[out_bytes++] = c;
            }
        }

        if (out_bytes) {
            break;
        }
    }
    return out_bytes;
}

static int telnet_control(struct dev *d, int op, int arg) {
    switch (op) {
        case TELNET_LOCAL_ENABLED:
            return option_enabled(TELNET_LOCAL, arg);
        case TELNET_REMOTE_ENABLED:
            return option_enabled(TELNET_REMOTE, arg);
        case TELNET_LOCAL_ENABLE:
            enable_option(telnet.fd, TELNET_LOCAL, arg);
            return 0;
        case TELNET_REMOTE_ENABLE:
            enable_option(telnet.fd, TELNET_REMOTE, arg);
            return 0;
    }
    return -1;
}

static const struct dev_ops telnet_ops = {
    .open = telnet_open,
    .close = telnet_close,
    .read = telnet_read,
    .write = telnet_write,
    .control = telnet_control,
};

void telnet_init(void) {
    dev_register("telnet", &telnet_ops, NULL);
}
