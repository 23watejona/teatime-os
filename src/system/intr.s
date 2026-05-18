
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
    rfi 3

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

