#include "string.h"
#include "uart.h"
#include "dev.h"
#include "tcp.h"
#include "http.h"
#include "pot.h"

#define HTCPCP_PORT 80
#define REQ_MAX 1024
#define RESP_MAX 1024
#define BODY_MAX 256
#define ALTERNATES_MAX 256

struct conn {
    struct http_request request;
    char req[REQ_MAX];
    char resp[RESP_MAX];
    char body[BODY_MAX];
};

static void begin(struct conn *c, const char *status) {
    kprintf_uart("htcpcp: %s\n", status);
    http_response_status(c->resp, status);
    c->body[0] = '\0';
}

static void alternates(struct conn *c) {
    char value[ALTERNATES_MAX];
    value[0] = '\0';
    for (int i = 0; i < POT_TEAS; i++) {
        if (i > 0)
            strcat(value, ", ");
        strcat(value, "{\"");
        strcat(value, pot_teas[i].uri);
        strcat(value, "\" {type message/teapot}}");
    }
    http_response_header(c->resp, "Alternates", value);
}

static void finish(struct conn *c) {
    http_response_header(c->resp, "Content-Type", "text/plain");
    http_response_header(c->resp, "Connection", "close");
    http_response_body(c->resp, c->body);
}

static void reply(struct conn *c, const char *status, const char *text) {
    begin(c, status);
    strcat(c->body, text);
    strcat(c->body, HTTP_CRLF);
    finish(c);
}

static void menu(struct conn *c) {
    begin(c, "200 OK");
    for (int i = 0; i < POT_TEAS; i++) {
        strcat(c->body, pot_teas[i].uri);
        strcat(c->body, HTTP_CRLF);
    }
    finish(c);
}

static void status(struct conn *c) {
    struct pot_status st;
    pot_status(&st);
    begin(c, "200 OK");
    if (st.state == POT_IDLE) {
        strcat(c->body, "idle");
    } else {
        strcat(c->body, "brewing ");
        strcat(c->body, pot_teas[st.tea].uri);
        strcat(c->body, " for ");
        itoau(st.elapsed_secs, c->body + strlen(c->body), 10);
        strcat(c->body, "s, strength ");
        itoa(st.strength, c->body + strlen(c->body), 10);
        strcat(c->body, "%");
    }
    strcat(c->body, HTTP_CRLF);
    finish(c);
}

static void brew(struct conn *c, int tea) {
    const char *additions = http_request_header(&c->request, "Accept-Additions");
    if (additions && *additions) {
        reply(c, "406 Not Acceptable", "no additions available");
        return;
    }
    if (strcmp(c->request.body, "start") == 0) {
        if (pot_start(tea) < 0) {
            reply(c, "503 Service Unavailable", "pot is busy");
            return;
        }
        begin(c, "200 OK");
        strcat(c->body, "brewing ");
        strcat(c->body, pot_teas[tea].uri);
        strcat(c->body, HTTP_CRLF);
        finish(c);
    } else if (strcmp(c->request.body, "stop") == 0) {
        struct pot_status st;
        pot_status(&st);
        if (pot_stop() < 0) {
            reply(c, "400 Bad Request", "nothing brewing");
            return;
        }
        begin(c, "200 OK");
        strcat(c->body, "served ");
        strcat(c->body, pot_teas[st.tea].uri);
        strcat(c->body, " at strength ");
        itoa(st.strength, c->body + strlen(c->body), 10);
        strcat(c->body, "%" HTTP_CRLF);
        finish(c);
    } else {
        reply(c, "400 Bad Request", "body must be start or stop");
    }
}

static void handle(struct conn *c) {
    const char *path = c->request.path;
    const char *method = c->request.method;
    int root = strcmp(path, "/") == 0;
    int tea = -1;
    for (int i = 0; i < POT_TEAS; i++)
        if (strcmp(path, pot_teas[i].uri) == 0)
            tea = i;
    if (!root && tea < 0) {
        reply(c, "404 Not Found", "no such tea");
        return;
    }

    if (strcmp(method, "GET") == 0) {
        if (root)
            menu(c);
        else
            status(c);
        return;
    }
    if (strcmp(method, "BREW") != 0 && strcmp(method, "POST") != 0) {
        reply(c, "501 Not Implemented", "method not implemented");
        return;
    }
    char *type = http_request_header(&c->request, "Content-Type");
    if (!type) {
        reply(c, "415 Unsupported Media Type", "Content-Type must be message/teapot");
        return;
    }
    char *params = strchr(type, ';');
    if (params)
        *params = '\0';
    int teapot = strcasecmp(type, "message/teapot") == 0;
    int coffeepot = strcasecmp(type, "message/coffeepot") == 0;
    if (!teapot && !coffeepot) {
        reply(c, "415 Unsupported Media Type", "Content-Type must be message/teapot");
        return;
    }
    if (root) {
        begin(c, "300 Multiple Options");
        alternates(c);
        for (int i = 0; i < POT_TEAS; i++) {
            strcat(c->body, pot_teas[i].uri);
            strcat(c->body, HTTP_CRLF);
        }
        finish(c);
        return;
    }
    if (coffeepot) {
        reply(c, "418 I'm a teapot", "this is a teapot");
        return;
    }
    brew(c, tea);
}

void htcpcp_proc(void) {
    pot_init();
    int listener = open("tcp", 0);
    if (listener < 0 || control(listener, TCP_LISTEN, HTCPCP_PORT) < 0) {
        kprintf_uart("htcpcp: listen failed\n");
        return;
    }
    struct conn c;
    while (1) {
        int fd = control(listener, TCP_ACCEPT, 0);
        if (fd < 0)
            continue;
        int n = read(fd, c.req, REQ_MAX - 1);
        if (n <= 0) {
            close(fd);
            continue;
        }
        c.req[n] = '\0';
        if (http_request_parse(c.req, &c.request) == 0)
            handle(&c);
        else
            reply(&c, "400 Bad Request", "bad request");
        write(fd, c.resp, strlen(c.resp));
        close(fd);
    }
}
