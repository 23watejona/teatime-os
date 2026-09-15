#include "string.h"
#include "uart.h"
#include "dev.h"
#include "gpio.h"
#include "tcp.h"
#include "proc.h"
#include "timer.h"
#include "http.h"

#define PULSE_PORT 80
#define REQ_MAX 512
#define RESP_MAX 256
#define PUMP_PIN 5
#define SOLENOID_PIN 4
#define PUMP_TICKS (3 * TICKS_PER_SEC)
#define SOLENOID_TICKS (1 * TICKS_PER_SEC)
static const unsigned char on = 1; // the MOSFET modules switch on high
static const unsigned char off = 0;

static int pump = -1;
static int solenoid = -1;

static void reply(char *resp, const char *status, const char *text) {
    kprintf_uart("pulse: %s %s", status, text);
    http_response_status(resp, status);
    http_response_header(resp, "Content-Type", "text/plain");
    http_response_header(resp, "Connection", "close");
    http_response_body(resp, text);
}

static void pulse(char *resp, int fd, unsigned int ticks, const char *done) {
    write(fd, &on, 1);
    sleep(ticks);
    write(fd, &off, 1);
    reply(resp, "200 OK", done);
}

static void handle(struct http_request *request, char *resp) {
    if (strcmp(request->path, "/pump") == 0)
        pulse(resp, pump, PUMP_TICKS, "pulsed pump for 3s" HTTP_CRLF);
    else if (strcmp(request->path, "/solenoid") == 0)
        pulse(resp, solenoid, SOLENOID_TICKS, "pulsed solenoid for 1s" HTTP_CRLF);
    else if (strcmp(request->path, "/") == 0)
        reply(resp, "200 OK", "/pump" HTTP_CRLF "/solenoid" HTTP_CRLF);
    else
        reply(resp, "404 Not Found", "no such load" HTTP_CRLF);
}

static int load_open(int pin) {
    int fd = open("gpio", pin);
    if (fd < 0)
        return -1;
    control(fd, GPIO_OUTPUT, 0);
    write(fd, &off, 1);
    return fd;
}

void pulse_proc(void) {
    pump = load_open(PUMP_PIN);
    solenoid = load_open(SOLENOID_PIN);
    if (pump < 0 || solenoid < 0) {
        kprintf_uart("pulse: gpio open failed\n");
        return;
    }
    int listener = open("tcp", 0);
    if (listener < 0 || control(listener, TCP_LISTEN, PULSE_PORT) < 0) {
        kprintf_uart("pulse: listen failed\n");
        return;
    }
    char req[REQ_MAX];
    char resp[RESP_MAX];
    struct http_request request;
    while (1) {
        int conn = control(listener, TCP_ACCEPT, 0);
        if (conn < 0)
            continue;
        int n = read(conn, req, REQ_MAX - 1);
        if (n <= 0) {
            close(conn);
            continue;
        }
        req[n] = '\0';
        if (http_request_parse(req, &request) == 0)
            handle(&request, resp);
        else
            reply(resp, "400 Bad Request", "bad request" HTTP_CRLF);
        write(conn, resp, strlen(resp));
        close(conn);
    }
}
