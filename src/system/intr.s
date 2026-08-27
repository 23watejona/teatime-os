
	.section	.SystemInfoVector.text,"ax",@progbits
	.align	4
sysInfoVector:
  wsr.excsave1	a0
  wsr.excsave2	a1
  movi.n	a0, 1
  wsr.exccause	a0
  call0	debug_handler
	
  .section	.DebugExceptionVector.text,"ax",@progbits
	.align	4
debugExceptionVector:
call0	debug_handler
rfi 2
  .section	.NMIExceptionVector.text,"ax",@progbits
	.align	4
NMIExceptionVector:
    wsr.EXCSAVE3 a0
    wsr.EXCSAVE2 a1
    j drive_nmi

  .section	.KernelExceptionVector.text,"ax",@progbits
	.align	4
KernelExceptionVector:
  j UserExceptionVector # no separation between user mode and kernel mode for now

  .section	.UserExceptionVector.text,"ax",@progbits
	.align	4
UserExceptionVector:
    wsr.EXCSAVE1 a0 # save a0
    call0 _create_intr_frame
    call0 _get_exccause # sets a2=exccause
    call0 syscall_handler
    call0 _restore_intr_frame
    rsr.EXCSAVE1 a0
    rfe

_get_exccause:
    rsr a2, EXCCAUSE
    ret

  .section	.DoubleExceptionVector.text,"ax",@progbits
	.align	4
DoubleExceptionVector:
  rsr a2, EPC1
  addi a2,a2,3
  wsr a2, EPC1
  xsr a2, EXCSAVE1
  call0 double_exc_handler 
  rfi 1

  .section	.iram1,"ax",@progbits
  .align	4
.global intr_enable
intr_enable:
  rsil a5, 2
  rsr.intenable a3
  or a4, a3, a2
  wsr.intenable a4
  wsr.ps a5
  rsync
  mov.n a2, a3
  ret.n

# Level-3 NMI dispatch. On entry the vector has stashed the interrupted a0 in
# EXCSAVE3 and a1 in EXCSAVE2. The handler runs on a dedicated stack (never the
# interrupted task's), clears PS.EXCM so a fault inside the C handler traps
# normally instead of double-faulting, preserves EPC3/EPS3 (rfi 3 reloads PS
# from EPS3) plus the level-1 exception state the C code may disturb, re-arms the
# NMI source as the very last write before rfi to prevent re-entry onto the
# single dedicated stack, and returns with rfi 3.
drive_nmi:
    movi a1, _nmi_frame
    rsr.EXCSAVE3 a0
    s32i a0, a1, 0x00
    rsr.EXCSAVE2 a0
    s32i a0, a1, 0x04
    s32i a2, a1, 0x08
    s32i a3, a1, 0x0c
    s32i a4, a1, 0x10
    s32i a5, a1, 0x14
    s32i a6, a1, 0x18
    s32i a7, a1, 0x1c
    s32i a8, a1, 0x20
    s32i a9, a1, 0x24
    s32i a10, a1, 0x28
    s32i a11, a1, 0x2c
    s32i a12, a1, 0x30
    s32i a13, a1, 0x34
    s32i a14, a1, 0x38
    s32i a15, a1, 0x3c
    rsr.epc3 a0
    s32i a0, a1, 0x40
    rsr.eps3 a0
    s32i a0, a1, 0x44
    rsr.epc1 a0
    s32i a0, a1, 0x48
    rsr.exccause a0
    s32i a0, a1, 0x4c
    rsr.excvaddr a0
    s32i a0, a1, 0x50
    rsr.excsave1 a0
    s32i a0, a1, 0x54
    rsr.sar a0
    s32i a0, a1, 0x58

    movi a0, 0x23 # excm stays clear, so a fault in the c handler traps instead of double-faulting
    wsr.ps a0
    rsync
    call0 nmi_handler

    movi a0, 0x33 # excm set, so nothing can trap in the middle of the restore
    wsr.ps a0
    rsync
    l32i a0, a1, 0x58
    wsr.sar a0
    l32i a0, a1, 0x54
    wsr.excsave1 a0
    l32i a0, a1, 0x50
    wsr.excvaddr a0
    l32i a0, a1, 0x4c
    wsr.exccause a0
    l32i a0, a1, 0x48
    wsr.epc1 a0
    l32i a0, a1, 0x44
    wsr.eps3 a0
    l32i a0, a1, 0x40
    wsr.epc3 a0

    movi a0, 0x3ff00000  # re-arm the NMI source last, after state is restored
    movi a2, 1
    s32i a2, a0, 0

    l32i a2, a1, 0x08
    l32i a3, a1, 0x0c
    l32i a4, a1, 0x10
    l32i a5, a1, 0x14
    l32i a6, a1, 0x18
    l32i a7, a1, 0x1c
    l32i a8, a1, 0x20
    l32i a9, a1, 0x24
    l32i a10, a1, 0x28
    l32i a11, a1, 0x2c
    l32i a12, a1, 0x30
    l32i a13, a1, 0x34
    l32i a14, a1, 0x38
    l32i a15, a1, 0x3c
    l32i a0, a1, 0x00
    l32i a1, a1, 0x04
    rfi 3
