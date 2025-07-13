	.section	.SystemInfoVector.text,"ax",@progbits
	.align	4
sysInfoVector:
  wsr.excsave1	a0
  wsr.excsave2	a1
  movi.n	a0, 1
  wsr.exccause	a0
  call0	print_stuff
	
  .section	.DebugExceptionVector.text,"ax",@progbits
	.align	4
debugExceptionVector:
call0	debug_handler
rfi 2
  .section	.NMIExceptionVector.text,"ax",@progbits
	.align	4
NMIExceptionVector:
  rsr a2, EPC1
  #call0	print_stuff
  addi a2,a2,3
  wsr a2, EPC1
  xsr a2, EXCSAVE1
  rfi 1
  
  .section	.KernelExceptionVector.text,"ax",@progbits
	.align	4
KernelExceptionVector:
  rsr a2, EPC1
  #call0	print_stuff
  addi a2,a2,3
  wsr a2, EPC1
  xsr a2, EXCSAVE1
  rfi 1

  .section	.UserExceptionVector.text,"ax",@progbits
	.align	4
UserExceptionVector:
  call0	syscall_handler
  rsr a2, EPC1
  addi a2,a2,3
  wsr a2, EPC1
  rfi 1

  .section	.DoubleExceptionVector.text,"ax",@progbits
	.align	4
DoubleExceptionVector:
  rsr a2, EPC1
  #call0	print_stuff
  addi a2,a2,3
  wsr a2, EPC1
  xsr a2, EXCSAVE1
  call0 double_exc_handler 
  rfi 1
