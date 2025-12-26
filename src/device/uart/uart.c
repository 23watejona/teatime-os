#include "stdarg.h"

#define READ_PERI_REG(addr)  (*((volatile unsigned int *)addr))
#define WRITE_PERI_REG(addr, val) (*((volatile unsigned int *)addr)) = (unsigned int)(val)

#define UART_STATUS 0x6000001C
#define UART_AUTOBAUD 0x60000018
#define UART_FIFO 0x60000000
#define UART_CONF0 0x60000020
#define UART_CLKDIV 0x60000014
#define UART_INT_CLR 0x60000010
#define UART_TXFIFO_CNT 0x000000FF
#define UART_TXFIFO_CNT_S 16

char *itoau(int, char *, int);
char *itoa(int, char *, int);

void kputc_uart(int c) {
 if (c == '\n') {
    kputc_uart('\r');
  }  
  while (1) {
        unsigned int fifo_cnt = READ_PERI_REG(UART_STATUS) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);

        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
            break;
    }

    WRITE_PERI_REG(UART_FIFO, c);
}

void kprintf_uart(char *f, ...) {
  va_list args;
  va_start(args, f);
  while ((unsigned int)*f != (unsigned int)'\0') {
    if (*f != '%') {
      kputc_uart(*f++);
    } else {
      switch (*(++f)) {
       case 'd':
          {
            int tmp_d = va_arg(args, int);
            char buf[12] = {0};
            kprintf_uart(itoa(tmp_d, buf, 10));
            ++f;
          }
          break;
       case 'u':
          {
            unsigned int tmp_d = va_arg(args, unsigned int);
            char buf[12] = {0};
            kprintf_uart(itoau(tmp_d, buf, 10));
            ++f;
          }
          break;
       case 'x':
          {
            unsigned int tmp_d = va_arg(args, unsigned int);
            char buf[12] = {0};
	    kprintf_uart("0x");
            kprintf_uart(itoau(tmp_d, buf, 16));
            ++f;
          }
          break;
	case 's':
	  {
	    char *str = va_arg(args, char *);
	    kprintf_uart(str);
	    ++f;
	  }
	  break;
        default:
          kputc_uart(*f++);
      }
    }
  }
}

