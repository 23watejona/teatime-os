#include "string.h"
#include "http.h"

static const char root[] = "/";

static char *header_value(const char *line, const char *name) {
    unsigned long n = strlen(name);
    if (strncasecmp(line, name, n) != 0 || line[n] != ':')
        return 0;
    const char *v = line + n + 1;
    while (*v == ' ')
        v++;
    return (char *) v;
}

int http_request_parse(char *buf, struct http_request *req) {
    char *blank = strstr(buf, HTTP_CRLF HTTP_CRLF);
    if (!blank)
        return -1;
    char *line_end = strstr(buf, HTTP_CRLF);
    char *sp1 = strchr(buf, ' ');
    if (!sp1 || sp1 > line_end)
        return -1;
    char *uri = sp1 + 1;
    char *sp2 = strchr(uri, ' ');
    if (!sp2 || sp2 > line_end)
        return -1;

    *sp1 = '\0';
    *sp2 = '\0';
    *line_end = '\0';
    char *q = strchr(uri, '?');
    if (q)
        *q = '\0';
    req->method = buf;
    req->path = uri;
    char *scheme = strstr(uri, "://");
    if (scheme) {
        char *slash = strchr(scheme + 3, '/');
        req->path = slash ? slash : root;
    }
    req->headers = line_end + 2;
    for (char *line = req->headers; line < blank; line += 2) {
        line = strstr(line, HTTP_CRLF);
        *line = '\0';
    }
    blank[2] = '\0';
    req->body = blank + 4;
    return 0;
}

char *http_request_header(const struct http_request *req, const char *name) {
    for (const char *line = req->headers; *line; line += strlen(line) + 2) {
        char *v = header_value(line, name);
        if (v)
            return v;
    }
    return 0;
}

void http_response_status(char *resp, const char *status) {
    strcpy(resp, "HTTP/1.1 ");
    strcat(resp, status);
    strcat(resp, HTTP_CRLF);
}

void http_response_header(char *resp, const char *name, const char *value) {
    strcat(resp, name);
    strcat(resp, ": ");
    strcat(resp, value);
    strcat(resp, HTTP_CRLF);
}

void http_response_body(char *resp, const char *body) {
    strcat(resp, "Content-Length: ");
    itoau(strlen(body), resp + strlen(resp), 10);
    strcat(resp, HTTP_CRLF HTTP_CRLF);
    strcat(resp, body);
}
