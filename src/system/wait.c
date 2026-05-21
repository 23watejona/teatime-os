void wait_us(unsigned int us) {
    unsigned int ticks = (us << 6) + (us << 4);
    unsigned int ccount_start;
    asm("rsr.ccount %0" : "=r" (ccount_start));
    // the iteration cap bounds the loop if ccount stops counting
    for (unsigned int i = 0; i < 100000000u; ++i) {
        unsigned int ccount_cur;
        asm("rsr.ccount %0" : "=r" (ccount_cur));
        if (ccount_cur - ccount_start >= ticks)
            break;
    }
}
