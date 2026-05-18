#ifndef TIMER_H
#define TIMER_H

struct frc1_t {
    struct {
        unsigned int data: 23;
        unsigned int reserved: 9;
    } load;
    
    struct {
        unsigned int data: 23;
        unsigned int reserved: 9;
    } count;

    struct {
        unsigned int div: 6;
        unsigned int reload: 1;
        unsigned int enable: 1;
        unsigned int intr_type: 1;
        unsigned int reserved: 23;
    } ctrl;

    struct {
        unsigned int clear: 1;
        unsigned int reserved: 31;
    } intr;
};

extern volatile struct frc1_t frc1;


_Static_assert(sizeof(struct frc1_t) == 16, "sizeof(struct frc1_t) != 16");

#endif // TIMER_H
