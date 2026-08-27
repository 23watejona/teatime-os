/*
Derived from esp8266.bootloader.ld in ESP8266_RTOS_SDK.
Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
Licensed under the Apache License, Version 2.0.
*/


/* Simplified memory map for the bootloader

   The main purpose is to make sure the bootloader can load into main memory
   without overwriting itself.
*/
MEMORY
{
  dport0_seg :    org = 0x3FF00000, len = 0x10

  /* All .data/.bss/heap are in this segment. Reserve 1KB for old boot or ROM boot */
  dram_seg  :    org = 0x3FFE8000, len = 0x18000

  /* Functions which are critical should be put in this segment. */
  iram_seg  :    org = 0x40100000, len = 0x8000

  /* cache window base plus the irom image's flash offset from the esptool write */
  irom_seg  :    org = 0x40210000, len = 0x80000
}

/*  Default entry point:  */
ENTRY(start);

SECTIONS
{
  .intvec :
  {
    _iram_start = ABSOLUTE(.);
    /* Vectors go to IRAM */
    _init_start = ABSOLUTE(.);
    KEEP(*(.SystemInfoVector.text));
    . = 0x10;
    KEEP(*(.DebugExceptionVector.text));
    . = 0x20;
    KEEP(*(.NMIExceptionVector.text));
    . = 0x30;
    KEEP(*(.KernelExceptionVector.text));
    . = 0x50;
    KEEP(*(.UserExceptionVector.text));
    . = 0x70;
    KEEP(*(.DoubleExceptionVector.text));

    *(.*Vector.literal)

    _init_end = ABSOLUTE(.);
  } > iram_seg

  /* IRAM: only what must not run from flash. C code opts in with IRAM_ATTR,
     the asm glue with .section .iram1. Everything else falls through to
     .irom0.text. */
  .text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
    *start.o(.literal .text .literal.* .text.*)
    *intr.o(.literal .text .literal.* .text.*)
    *intr_util.o(.literal .text .literal.* .text.*)
    *ctxsw.o(.literal .text .literal.* .text.*)
    *_set_vec_base.o(.literal .text .literal.* .text.*)
    *disable.o(.literal .text .literal.* .text.*)
    *enable.o(.literal .text .literal.* .text.*)
    *wifi_fiq.o(.literal .text .literal.* .text.*)
    *handle_cputimer_intr.o(.literal .text .literal.* .text.*)
    *(.iram1 .iram1.*) /* catch stray IRAM_ATTR */
    *(.fini.literal)
    *(.fini)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } > iram_seg

  .irom0.text :
  {
    _irom_start = ABSOLUTE(.);
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    _irom_end = ABSOLUTE(.);
  } > irom_seg

  /* Shared RAM */
  .bss (NOLOAD) :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } >dram_seg

  .data :
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    _data_end = ABSOLUTE(.);
  } >dram_seg

  .rodata :
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    . = ALIGN(4);
    _heap_start = ABSOLUTE(.);
  } >dram_seg
}

PROVIDE ( _heap_end = 0x3FFFFC00);
