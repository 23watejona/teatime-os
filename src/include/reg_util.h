#define WRITE_REG(addr, val) (*((volatile unsigned int *) addr) = val)
#define READ_REG(addr) (*((volatile unsigned int *) addr))
#define WRITE_REG_MASK(addr, mask) WRITE_REG(addr, (READ_REG(addr) | (mask)))
#define WRITE_REG_UNMASK(addr, mask) WRITE_REG(addr, (READ_REG(addr) & (0xFFFFFFFF - mask)))
#define WRITE_REG_RMW(addr, keep_mask, set_bits) \
    WRITE_REG(addr, (READ_REG(addr) & (keep_mask)) | (set_bits))

#define BUSY_WAIT() do { \
        for (int i = 0; i < 5000000; ++i) { \
            __asm__(""); \
        } \
    } while (0)
