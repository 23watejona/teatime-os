#include "string.h"
#include "uart.h"
#include "dev.h"
#include "tcp.h"
#include "pot.h"

#define HTCPCP_PORT 80
#define REQ_MAX 1024
#define RESP_MAX 1024
#define BODY_MAX 256

#define CRLF "\r\n"

static unsigned char req[REQ_MAX];
static unsigned int req_len;
static unsigned int req_body;

static char resp[RESP_MAX];
static unsigned int resp_len;
static char body[BODY_MAX];
static unsigned int body_len;

static int lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int equal_ci(const unsigned char *s, unsigned int n, const char *word) {
    if (strlen(word) != n)
        return 0;
    for (unsigned int i = 0; i < n; i++)
        if (lower(s[i]) != lower(word[i]))
            return 0;
    return 1;
}

static int equal(const unsigned char *s, unsigned int n, const char *word) {
    return strlen(word) == n && memcmp(s, word, n) == 0;
}

static int find(const unsigned char *s, unsigned int n, const char *word) {
    unsigned int wn = strlen(word);
    for (unsigned int i = 0; i + wn <= n; i++)
        if (memcmp(s + i, word, wn) == 0)
            return (int) i;
    return -1;
}

static unsigned int parse_uint(const unsigned char *s, unsigned int n) {
    unsigned int v = 0;
    for (unsigned int i = 0; i < n && s[i] >= '0' && s[i] <= '9'; i++)
        v = v * 10 + (s[i] - '0');
    return v;
}

static int header(const char *name, const unsigned char **val, unsigned int *vlen) {
    unsigned int pos = find(req, req_body, CRLF) + 2;
    unsigned int nlen = strlen(name);
    while (pos < req_body) {
        unsigned int end = pos + find(req + pos, req_body - pos, CRLF);
        if (end == pos)
            break;
        if (end - pos > nlen && req[pos + nlen] == ':' && equal_ci(req + pos, nlen, name)) {
            unsigned int v = pos + nlen + 1;
            while (v < end && req[v] == ' ')
                v++;
            *val = req + v;
            *vlen = end - v;
            return 1;
        }
        pos = end + 2;
    }
    return 0;
}

static void put(const char *s) {
    unsigned int n = strlen(s);
    if (n > RESP_MAX - resp_len)
        n = RESP_MAX - resp_len;
    memcpy(resp + resp_len, s, n);
    resp_len += n;
}

static void body_put(const char *s) {
    unsigned int n = strlen(s);
    if (n > BODY_MAX - body_len)
        n = BODY_MAX - body_len;
    memcpy(body + body_len, s, n);
    body_len += n;
}

static void body_put_int(int v) {
    char digits[12];
    body_put(itoa(v, digits, 10));
}

static void begin(const char *status) {
    kprintf_uart("htcpcp: %s\n", status);
    resp_len = 0;
    body_len = 0;
    put("HTTP/1.1 ");
    put(status);
    put(CRLF);
}

static void alternates(void) {
    put("Alternates: ");
    for (int i = 0; i < POT_TEAS; i++) {
        if (i > 0)
            put(", ");
        put("{\"");
        put(pot_teas[i].uri);
        put("\" {type message/teapot}}");
    }
    put(CRLF);
}

static void finish(void) {
    char digits[12];
    put("Content-Type: text/plain" CRLF "Content-Length: ");
    put(itoau(body_len, digits, 10));
    put(CRLF "Connection: close" CRLF CRLF);
    if (body_len > RESP_MAX - resp_len)
        body_len = RESP_MAX - resp_len;
    memcpy(resp + resp_len, body, body_len);
    resp_len += body_len;
}

static void reply(const char *status, const char *text) {
    begin(status);
    body_put(text);
    body_put(CRLF);
    finish();
}

static void menu(void) {
    begin("200 OK");
    for (int i = 0; i < POT_TEAS; i++) {
        body_put(pot_teas[i].uri);
        body_put(CRLF);
    }
    finish();
}

static void status(void) {
    struct pot_status st;
    pot_status(&st);
    begin("200 OK");
    if (st.state == POT_IDLE) {
        body_put("idle");
    } else {
        body_put("brewing ");
        body_put(pot_teas[st.tea].uri);
        body_put(" for ");
        body_put_int((int) st.elapsed_secs);
        body_put("s, strength ");
        body_put_int(st.strength);
        body_put("%");
    }
    body_put(CRLF);
    finish();
}

static void brew(int tea) {
    const unsigned char *v;
    unsigned int vn;
    if (header("Accept-Additions", &v, &vn) && vn > 0) {
        reply("406 Not Acceptable", "no additions available");
        return;
    }
    const unsigned char *cmd = req + req_body;
    unsigned int cmd_len = req_len - req_body;
    if (equal(cmd, cmd_len, "start")) {
        if (pot_start(tea) < 0) {
            reply("503 Service Unavailable", "pot is busy");
            return;
        }
        begin("200 OK");
        body_put("brewing ");
        body_put(pot_teas[tea].uri);
        body_put(CRLF);
        finish();
    } else if (equal(cmd, cmd_len, "stop")) {
        struct pot_status st;
        pot_status(&st);
        if (pot_stop() < 0) {
            reply("400 Bad Request", "nothing brewing");
            return;
        }
        begin("200 OK");
        body_put("served ");
        body_put(pot_teas[st.tea].uri);
        body_put(" at strength ");
        body_put_int(st.strength);
        body_put("%" CRLF);
        finish();
    } else {
        reply("400 Bad Request", "body must be start or stop");
    }
}

static void handle(void) {
    unsigned int line = (unsigned int) find(req, req_len, CRLF);
    int sp1 = find(req, line, " ");
    if (sp1 < 0) {
        reply("400 Bad Request", "malformed request line");
        return;
    }
    unsigned int uri = sp1 + 1;
    int sp2 = find(req + uri, line - uri, " ");
    if (sp2 < 0) {
        reply("400 Bad Request", "malformed request line");
        return;
    }
    unsigned int uri_len = sp2;
    int q = find(req + uri, uri_len, "?");
    if (q >= 0)
        uri_len = q;
    int scheme = find(req + uri, uri_len, "://");
    if (scheme >= 0) {
        unsigned int host = uri + scheme + 3;
        int slash = find(req + host, uri + uri_len - host, "/");
        uri_len = slash < 0 ? 0 : uri + uri_len - (host + slash);
        uri = host + slash;
    }
    int root = uri_len == 0 || equal(req + uri, uri_len, "/");
    int tea = -1;
    for (int i = 0; i < POT_TEAS; i++)
        if (equal(req + uri, uri_len, pot_teas[i].uri))
            tea = i;
    if (!root && tea < 0) {
        reply("404 Not Found", "no such tea");
        return;
    }

    const unsigned char *method = req;
    unsigned int method_len = sp1;
    if (equal(method, method_len, "GET")) {
        if (root)
            menu();
        else
            status();
        return;
    }
    if (!equal(method, method_len, "BREW") && !equal(method, method_len, "POST")) {
        reply("501 Not Implemented", "method not implemented");
        return;
    }
    const unsigned char *type;
    unsigned int type_len;
    if (!header("Content-Type", &type, &type_len)) {
        reply("415 Unsupported Media Type", "Content-Type must be message/teapot");
        return;
    }
    int params = find(type, type_len, ";");
    if (params >= 0)
        type_len = params;
    int teapot = equal_ci(type, type_len, "message/teapot");
    int coffeepot = equal_ci(type, type_len, "message/coffeepot");
    if (!teapot && !coffeepot) {
        reply("415 Unsupported Media Type", "Content-Type must be message/teapot");
        return;
    }
    if (root) {
        begin("300 Multiple Options");
        alternates();
        for (int i = 0; i < POT_TEAS; i++) {
            body_put(pot_teas[i].uri);
            body_put(CRLF);
        }
        finish();
        return;
    }
    if (coffeepot) {
        reply("418 I'm a teapot", "this is a teapot");
        return;
    }
    brew(tea);
}

static int request_complete(void) {
    int end = find(req, req_len, CRLF CRLF);
    if (end < 0)
        return 0;
    req_body = end + 4;
    const unsigned char *v;
    unsigned int vn;
    unsigned int content_len = header("Content-Length", &v, &vn) ? parse_uint(v, vn) : 0;
    return req_len - req_body >= content_len;
}

void htcpcp_proc(void) {
    int listener = open("tcp", 0);
    if (listener < 0 || control(listener, TCP_LISTEN, HTCPCP_PORT) < 0) {
        kprintf_uart("htcpcp: listen failed\n");
        return;
    }
    while (1) {
        int conn = control(listener, TCP_ACCEPT, 0);
        if (conn < 0)
            continue;
        req_len = 0;
        int n = 1;
        while (n > 0 && req_len < REQ_MAX && !request_complete()) {
            n = read(conn, req + req_len, REQ_MAX - req_len);
            if (n > 0)
                req_len += n;
        }
        if (n < 0) {
            close(conn);
            continue;
        }
        if (request_complete())
            handle();
        else
            reply("400 Bad Request", "incomplete request");
        write(conn, resp, resp_len);
        close(conn);
    }
}
