#include "def.h"
#include "dev.h"
#include "string.h"
#include "proc.h"

#define NDEV 16

static struct dev devtab[NDEV];

static int devtab_mutex;

void dev_init(void) {
    devtab_mutex = mutex_create();
}

struct dev *dev_register(const char *name, const struct dev_ops *ops, void *state) {
    struct dev *d = NULL;
    mutex_lock(devtab_mutex);
    for (int i = 0; i < NDEV; i++) {
        if (!devtab[i].name) {
            d = &devtab[i];
            d->name = name;
            d->ops = ops;
            d->state = state;
            d->cond = cond_create();
            break;
        }
    }
    mutex_unlock(devtab_mutex);
    return d;
}

int dev_alloc(const char *name) {
    int fd = -1;
    mutex_lock(devtab_mutex);
    for (int i = 0; i < NDEV; i++) {
        if (devtab[i].name && !devtab[i].used && strcmp(devtab[i].name, name) == 0) {
            devtab[i].used = 1;
            fd = i;
            break;
        }
    }
    mutex_unlock(devtab_mutex);
    return fd;
}

static void release(int fd) {
    mutex_lock(devtab_mutex);
    devtab[fd].used = 0;
    mutex_unlock(devtab_mutex);
}

static struct dev *lookup(int fd) {
    if (fd < 0 || fd >= NDEV || !devtab[fd].used)
        return NULL;
    return &devtab[fd];
}

int open(const char *name, int arg) {
    int fd = dev_alloc(name);
    if (fd < 0)
        return -1;
    struct dev *d = &devtab[fd];
    if (d->ops->open && d->ops->open(d, arg) < 0) {
        release(fd);
        return -1;
    }
    return fd;
}

int close(int fd) {
    struct dev *d = lookup(fd);
    if (!d)
        return -1;
    if (d->ops->close && d->ops->close(d) < 0)
        return -1;
    release(fd);
    return 0;
}

int read(int fd, void *buf, unsigned int n) {
    struct dev *d = lookup(fd);
    if (!d || !d->ops->read)
        return -1;
    if (n == 0)
        return 0;
    return d->ops->read(d, buf, n);
}

int write(int fd, const void *buf, unsigned int n) {
    struct dev *d = lookup(fd);
    if (!d || !d->ops->write)
        return -1;
    return d->ops->write(d, buf, n);
}

int control(int fd, int op, int arg) {
    struct dev *d = lookup(fd);
    if (!d || !d->ops->control)
        return -1;
    return d->ops->control(d, op, arg);
}
