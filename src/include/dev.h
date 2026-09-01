#ifndef DEV_H
#define DEV_H

struct dev;

struct dev_ops {
    int (*open)(struct dev *d, int arg);
    int (*close)(struct dev *d);
    int (*read)(struct dev *d, void *buf, unsigned int n);
    int (*write)(struct dev *d, const void *buf, unsigned int n);
    int (*control)(struct dev *d, int op, int arg);
};

struct dev {
    const char *name;
    const struct dev_ops *ops;
    void *state;
    int used;
};

/* Every call may block: process context only. A descriptor is a global index,
   usable by any process. An op the device does not provide returns -1. */
int open(const char *name, int arg);
int close(int fd);
/* Blocks until at least one byte, then returns 1..n; 0 is end of stream,
   -1 an error. n == 0 returns 0 without blocking. */
int read(int fd, void *buf, unsigned int n);
int write(int fd, const void *buf, unsigned int n);
int control(int fd, int op, int arg);

/* Drivers only: claims a free row of that name (an accepted connection),
   bypassing the row's open op. */
int dev_alloc(const char *name);

void dev_init(void);

#endif
