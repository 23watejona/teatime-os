    .section .iram1,"ax",@progbits
    .align 4
    .global disable
disable:
    rsil a2, 15
    ret
