/* Optional GAS version of fury_dot3_asm (SysV AMD64).
 * The build uses the NASM .asm file by default when FURY_HAS_ASM is on.
 */
#if defined(__x86_64__) || defined(_M_X64)
    .text
    .globl fury_dot3_asm
    .type fury_dot3_asm, @function
fury_dot3_asm:
    movss   (%rdi), %xmm0
    mulss   (%rsi), %xmm0
    movss   4(%rdi), %xmm1
    mulss   4(%rsi), %xmm1
    addss   %xmm1, %xmm0
    movss   8(%rdi), %xmm1
    mulss   8(%rsi), %xmm1
    addss   %xmm1, %xmm0
    ret
    .size fury_dot3_asm, .-fury_dot3_asm
#endif
