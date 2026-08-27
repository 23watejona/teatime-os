#ifndef RNG_H
#define RNG_H

/* 32 bits of hardware entropy per call. Valid only while the radio is up. */
unsigned int rng_read(void);
void rng_fill(unsigned char *buf, unsigned int len);

#endif
