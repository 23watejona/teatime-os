// gcc emits calls to these for / and % because the lx106 has no divider

static void udivmod(unsigned long num, unsigned long den, unsigned long *quot, unsigned long *rem) {
    unsigned long q = 0;
    unsigned long r = 0;

    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((num >> i) & 1);
        if (r >= den) {
            r -= den;
            q |= 1ul << i;
        }
    }
    *quot = q;
    *rem = r;
}

long __divsi3(long a, long b) {
    unsigned long ua = a < 0 ? -(unsigned long)a : a;
    unsigned long ub = b < 0 ? -(unsigned long)b : b;
    unsigned long q;
    unsigned long r;
    udivmod(ua, ub, &q, &r);
    return (a < 0) != (b < 0) ? -(long)q : (long)q;
}

long __modsi3(long a, long b) {
    unsigned long ua = a < 0 ? -(unsigned long)a : a;
    unsigned long ub = b < 0 ? -(unsigned long)b : b;
    unsigned long q;
    unsigned long r;
    udivmod(ua, ub, &q, &r);
    return a < 0 ? -(long)r : (long)r;
}
