void wait_us(unsigned int us) {
    unsigned int ticks = (us << 6) + (us << 4);
    unsigned int ccount_start;
    unsigned int ccount_end;
    asm("rsr.ccount %0" : "=r" (ccount_start));
    ccount_end = ccount_start + ticks;
    while (({ 
        unsigned int ccount_cur;
        asm("rsr.ccount %0" : "=r" (ccount_cur));
        ccount_cur;
    }) < ccount_end);
}
