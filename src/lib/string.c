#include "string.h"

void *memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s)
        for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    else
        for (unsigned long i = n; i > 0; i--) d[i - 1] = s[i - 1];
    return dst;
}

void *memset(void *dst, int c, unsigned long n) {
    unsigned char *d = dst;
    for (unsigned long i = 0; i < n; i++) d[i] = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, unsigned long n) {
    const unsigned char *x = a, *y = b;
    for (unsigned long i = 0; i < n; i++)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

unsigned long strlen(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}
