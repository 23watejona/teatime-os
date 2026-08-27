#ifndef NULL
#define NULL (0)
#endif

typedef unsigned char u8;

/* code that runs with the flash cache off or from the NMI */
#define IRAM_ATTR __attribute__((section(".iram1")))
#define INT_MIN (-2147483648)
#define INT_MAX (+2147483647)
