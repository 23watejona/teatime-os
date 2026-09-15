#ifndef STRING_H
#define STRING_H

void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
void *memset(void *dst, int c, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);
unsigned long strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, unsigned long n);
char *strchr(const char *s, int c);
char *strstr(const char *hay, const char *needle);
int tolower(int c);
char *itoa(int num, char *buf, int base);
char *itoau(unsigned int num, char *buf, int base);

#endif
