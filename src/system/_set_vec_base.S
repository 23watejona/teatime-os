#define VEC_BASE (0x40100000)
    .text
    .global _set_vec_base
    .align 4
_set_vec_base:
    addi sp, sp, -16
    s32i a2, sp, 0
    movi a2, VEC_BASE
    wsr.vecbase a2
    l32i a2, sp, 0
    addi sp, sp, 16
