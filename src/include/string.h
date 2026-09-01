#ifndef STRING_H
#define STRING_H

void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
void *memset(void *dst, int c, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);
unsigned long strlen(const char *s);
char *itoa(int num, char *buf, int base);
char *itoau(unsigned int num, char *buf, int base);

#endif
