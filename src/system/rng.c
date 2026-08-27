#include "reg_util.h"
#include "rng.h"

#define WDEV_RNG 0x3ff20e44u

unsigned int rng_read(void) {
    return READ_REG(WDEV_RNG);
}

void rng_fill(unsigned char *buf, unsigned int len) {
    unsigned int r = 0;
    for (unsigned int i = 0; i < len; i++) {
        if ((i & 3) == 0)
            r = READ_REG(WDEV_RNG);
        buf[i] = r;
        r >>= 8;
    }
}
