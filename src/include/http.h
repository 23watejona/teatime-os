#ifndef HTTP_H
#define HTTP_H

#define HTTP_CRLF "\r\n"

/* Each field is a NUL-terminated string inside the buffer handed to
   http_request_parse, which writes the terminators in place. path is the URI
   after the host without any ?query, "/" if the URI has none. */
struct http_request {
    const char *method;
    const char *path;
    char *headers;
    const char *body;
};

/* -1 if buf has no request line or no blank line */
int http_request_parse(char *buf, struct http_request *req);
char *http_request_header(const struct http_request *req, const char *name);

/* resp is strcat'd with no bound, so the caller sizes it. http_response_status
   starts it over; http_response_body adds Content-Length itself. */
void http_response_status(char *resp, const char *status);
void http_response_header(char *resp, const char *name, const char *value);
void http_response_body(char *resp, const char *body);

#endif
