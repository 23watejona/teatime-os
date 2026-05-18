#define VEC_BASE (0x40100000)
    .text
    .global _set_vec_base
    .align 4
_set_vec_base:
    movi a2, VEC_BASE
    wsr.vecbase a2
    ret
