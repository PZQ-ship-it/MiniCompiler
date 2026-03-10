    .section .text
    .globl main
main:
    addi sp, sp, -24
    sw ra, 20(sp)
    sw fp, 16(sp)
    addi fp, sp, 24
    li t2, -40
    lw t2, 0(sp)
    add t2, sp, t2
    sw t2, 4(sp)
    li t2, 4
    sw t2, 8(t2)
    lw t2, 8(t2)
    mv a0, t2
    call write
    li a0, 0
    j main.ret
main.ret:
    lw ra, 20(sp)
    lw fp, 16(sp)
    addi sp, sp, 24
    ret
