    .text
    .align 4
    .global ctxsw
# frame (moving backwards from sp):
# epc1 / return address 0x0
# ps 0x4
# sar 0x8
# intenable 0xc
# a0-a15 0x10-0x50
# 4 bytes each, 80 total, 80 is 16 byte aligned so we're all good
# assume we come in with interrupts off
# ctxsw(&sp1, &sp2)
ctxsw:

    # allocate interrupt frame
    addi sp, sp, -80

    # save new stack pointer to &sp1 (*sp1 = new stack pointer)
    s32i sp, a2, 0
    
    # spill a0 so we have a scratch register
    s32i a0, sp, 0x10

    # save special registers
    rsr.epc1 a0
    s32i a0, sp, 0x0
    rsr.ps a0
    s32i a0, sp, 0x4
    rsr.sar a0
    s32i a0, sp, 0x8
    rsr.intenable a0
    s32i a0, sp, 0xc
    
    # save general registers
    s32i a1, sp, 0x14
    s32i a2, sp, 0x18
    s32i a3, sp, 0x1c
    s32i a4, sp, 0x20
    s32i a5, sp, 0x24
    s32i a6, sp, 0x28
    s32i a7, sp, 0x2c
    s32i a8, sp, 0x30
    s32i a9, sp, 0x34
    s32i a10, sp, 0x38
    s32i a11, sp, 0x3c
    s32i a12, sp, 0x40
    s32i a13, sp, 0x44
    s32i a14, sp, 0x48
    s32i a15, sp, 0x4c
    

    # switch to new stack
    l32i sp, a3, 0x0

    #restore general registers
    l32i a1, sp, 0x14
    l32i a2, sp, 0x18
    l32i a3, sp, 0x1c
    l32i a4, sp, 0x20
    l32i a5, sp, 0x24
    l32i a6, sp, 0x28
    l32i a7, sp, 0x2c
    l32i a8, sp, 0x30
    l32i a9, sp, 0x34
    l32i a10, sp, 0x38
    l32i a11, sp, 0x3c
    l32i a12, sp, 0x40
    l32i a13, sp, 0x44
    l32i a14, sp, 0x48
    l32i a15, sp, 0x4c
    

    # restore special registers
    l32i a0, sp, 0x0
    wsr.epc1 a0
    l32i a0, sp, 0x4
    wsr.sar a0
    l32i a0, sp, 0xc
    wsr.intenable a0
    l32i a0, sp, 0x8
    wsr.ps a0
    rsync
    l32i a0,sp, 0x10
    
    # now restore return address 
    l32i a0,sp, 0x10
    
    # invalidate interrupt frame
    addi sp, sp, 80

    # return from context switch
    ret
