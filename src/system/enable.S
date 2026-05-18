    .section .iram1,"ax",@progbits
    .align 4
    .global enable
enable:
    wsr.ps a2
    ret
