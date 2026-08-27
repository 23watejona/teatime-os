    .section .iram1,"ax",@progbits
    .align 4

    .macro SAVE_AREGS
    s32i a1,  sp, 0x14
    s32i a2,  sp, 0x18
    s32i a3,  sp, 0x1c
    s32i a4,  sp, 0x20
    s32i a5,  sp, 0x24
    s32i a6,  sp, 0x28
    s32i a7,  sp, 0x2c
    s32i a8,  sp, 0x30
    s32i a9,  sp, 0x34
    s32i a10, sp, 0x38
    s32i a11, sp, 0x3c
    s32i a12, sp, 0x40
    s32i a13, sp, 0x44
    s32i a14, sp, 0x48
    s32i a15, sp, 0x4c
    .endm

    .macro REST_AREGS
    l32i a1,  sp, 0x14
    l32i a2,  sp, 0x18
    l32i a3,  sp, 0x1c
    l32i a4,  sp, 0x20
    l32i a5,  sp, 0x24
    l32i a6,  sp, 0x28
    l32i a7,  sp, 0x2c
    l32i a8,  sp, 0x30
    l32i a9,  sp, 0x34
    l32i a10, sp, 0x38
    l32i a11, sp, 0x3c
    l32i a12, sp, 0x40
    l32i a13, sp, 0x44
    l32i a14, sp, 0x48
    l32i a15, sp, 0x4c
    .endm

    .macro SAVE_SREGS_L1
    rsr.epc1      a0
    s32i a0, sp, 0x00
    rsr.ps        a0
    s32i a0, sp, 0x04
    rsr.sar       a0
    s32i a0, sp, 0x08
    rsr.intenable a0
    s32i a0, sp, 0x0c
    .endm

    .macro REST_SREGS_L1
    l32i a0, sp, 0x00
    wsr.epc1      a0
    l32i a0, sp, 0x08
    wsr.sar       a0
    l32i a0, sp, 0x0c
    wsr.intenable a0
    l32i a0, sp, 0x04
    wsr.ps        a0
    rsync
    .endm

    .macro SAVE_SREGS_NMI
    rsr.epc1      a0
    s32i a0, sp, 0x00
    rsr.exccause  a0
    s32i a0, sp, 0x04
    rsr.sar       a0
    s32i a0, sp, 0x08
    rsr.intenable a0
    s32i a0, sp, 0x0c
    rsr.excvaddr  a0
    s32i a0, sp, 0x50
    rsr.excsave1  a0
    s32i a0, sp, 0x54
    .endm

    .macro REST_SREGS_NMI
    l32i a0, sp, 0x54
    wsr.excsave1  a0
    l32i a0, sp, 0x00
    wsr.epc1      a0
    l32i a0, sp, 0x08
    wsr.sar       a0
    l32i a0, sp, 0x0c
    wsr.intenable a0
    .endm


    .global ctxsw
ctxsw:
    addi sp, sp, -80
    s32i sp, a2, 0
    s32i a0, sp, 0x10
    SAVE_SREGS_L1
    SAVE_AREGS

    l32i sp, a3, 0

    REST_AREGS
    REST_SREGS_L1
    l32i a0, sp, 0x10
    addi sp, sp, 80
    ret


    .align 4
    .global _create_intr_frame
_create_intr_frame:
    addi sp, sp, -80
    s32i a0, sp, 0x10
    SAVE_SREGS_L1
    SAVE_AREGS
    l32i a0, sp, 0x10
    ret

    .align 4
    .global _restore_intr_frame
_restore_intr_frame:
    s32i a0, sp, 0x10
    REST_AREGS
    REST_SREGS_L1
    l32i a0, sp, 0x10
    addi sp, sp, 80
    ret


    # Dedicated level-3 NMI stack. drive_nmi (intr.s) uses _nmi_frame as both the
    # HESF save area (offsets 0x00..0x5c) and the C handler's stack pointer, which
    # grows DOWN into the reserved space below it. Never the interrupted task's sp.
    .section .bss
    .align 16
_nmi_stack_bottom:
    .space 1024
    .global _nmi_frame
_nmi_frame:
    .space 0x60
