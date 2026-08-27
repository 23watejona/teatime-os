
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
    call0 _get_exccause
    mov.n a3, a1
    call0 syscall_handler
    call0 _restore_intr_frame
    rsr.EXCSAVE1 a0
    rfe

    .align 4 # call0 needs a 4-aligned target or gas expands it to l32r+callx0,
             # and vector literals land after the vector, out of l32r's reach
_get_exccause:
    rsr a2, EXCCAUSE
    ret

  .section	.DoubleExceptionVector.text,"ax",@progbits
	.align	4
DoubleExceptionVector:
    # printing would fault again, so store breadcrumbs and let the wdt reset
    # the stores live out in iram1, since literals placed after the vector are out of l32r's reach
    j double_exc_halt

  .section	.iram1,"ax",@progbits
  .align	4
double_exc_halt:
    movi a0, 0x60001200 # rtc ram survives the reset, so start() can print it
    rsr.epc1 a2
    s32i a2, a0, 4
    rsr.exccause a2
    s32i a2, a0, 8
    # epc1 is stale on a double fault, so depc holds the faulting pc
    rsr.depc a2
    s32i a2, a0, 0xc
    rsr.excvaddr a2
    s32i a2, a0, 0x10
1:  j 1b

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

# Level-3 NMI dispatch. Vector stashes interrupted a0/a1 in EXCSAVE3/2; a fresh
# entry saves the rest in the single static _nmi_frame and runs C with EXCM
# clear. This is a true NMI (fires at any PS, arm gate never auto-disables), so
# an event rising after the drain can nest a second entry any time before the
# rfi. The entry guard makes that harmless:
#   EPC3 in the restore block  -> rerun the restore (it only reads the frame)
#   _nmi_active                -> resume the interrupted point via EXCSAVE3/2
#   otherwise                  -> fresh entry
# A bounce consumes the edge without draining; the tick's gate pulse remakes it.
# No nest can land before the drain: the line is still high, so no new edge.
drive_nmi:
    rsr.epc3 a0
    movi a1, .Lnmi_restore
    bltu a0, a1, .Lnmi_chk_active
    movi a1, .Lnmi_restore_end
    bltu a0, a1, .Lnmi_restore
.Lnmi_chk_active:
    movi a0, _nmi_active
    l32i a0, a0, 0
    beqz a0, .Lnmi_fresh
    rsr.EXCSAVE3 a0
    rsr.EXCSAVE2 a1
    rfi 3

.Lnmi_fresh:
    movi a0, _nmi_active
    movi a1, 1
    s32i a1, a0, 0
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
    movi a2, 0x60001200 # rtc ram survives the reset, so start() can print it
    s32i a0, a2, 0
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

    # bounce restart point: everything to the rfi only reads _nmi_frame, so rerunning from here is safe
.Lnmi_restore:
    movi a1, _nmi_frame
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
    movi a0, _nmi_active
    movi a2, 0
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
.Lnmi_restore_end:
