; fury_dot3_asm — float dot product of two float[3] vectors
; SysV AMD64:  rdi = a, rsi = b, xmm0 = return
; Win64:       rcx = a, rdx = b, xmm0 = return
;
; result = a[0]*b[0] + a[1]*b[1] + a[2]*b[2]

default rel
bits 64

section .text

%ifdef WIN64
global fury_dot3_asm
fury_dot3_asm:
    ; rcx = a*, rdx = b*
    movss   xmm0, [rcx]
    mulss   xmm0, [rdx]
    movss   xmm1, [rcx + 4]
    mulss   xmm1, [rdx + 4]
    addss   xmm0, xmm1
    movss   xmm1, [rcx + 8]
    mulss   xmm1, [rdx + 8]
    addss   xmm0, xmm1
    ret
%else
global fury_dot3_asm:function
fury_dot3_asm:
    ; rdi = a*, rsi = b*
    movss   xmm0, [rdi]
    mulss   xmm0, [rsi]
    movss   xmm1, [rdi + 4]
    mulss   xmm1, [rsi + 4]
    addss   xmm0, xmm1
    movss   xmm1, [rdi + 8]
    mulss   xmm1, [rsi + 8]
    addss   xmm0, xmm1
    ret
%endif
