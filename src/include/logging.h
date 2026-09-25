#ifndef LOGGING_H
#define LOGGING_H

#include "uart.h"

enum log_level {
    ERROR,
    WARNING,
    INFO,
    DEBUG
};

#ifndef LOG_LEVEL
#define LOG_LEVEL INFO
#endif

#define kernel_debug(format, ...) do {\
    if (LOG_LEVEL == DEBUG) {\
        kprintf_uart(format,##__VA_ARGS__);\
    }\
} while (0)

#endif // LOGGING_H
