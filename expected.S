.section .text
    .globl main
main:
    addi sp, sp, -28
    sw ra, 24(sp)
    sw fp, 20(sp)
    addi fp, sp, 28
    call read
    mv t2, a0               # 使用t2接收返回值
    sw t2, 0(sp)            # 保存第一个值到栈
    call read
    mv t2, a0               # 使用t2接收第二个返回值
    sw t2, 4(sp)            # 保存第二个值到栈
    lw t0, 0(sp)            # 加载第一个值到t0
    lw t2, 4(sp)            # 加载第二个值到t2
    add t2, t0, t2          # 计算和，结果存在t2
    mv a0, t2               # 将结果移到a0用于调用write
    call write
    li a0, 0
    j main.ret
main.ret:
    lw ra, 24(sp)
    lw fp, 20(sp)
    addi sp, sp, 28
    ret