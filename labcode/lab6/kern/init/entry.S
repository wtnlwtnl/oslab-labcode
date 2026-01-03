#include <mmu.h>
#include <memlayout.h>

    # 必须使用这个段名以匹配链接脚本
    .section .text.kern_entry,"ax",%progbits
    .globl kern_entry
kern_entry:
    # 保存参数
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)

    # 计算页表物理地址
    la      t0, boot_page_table_sv39
    # 计算偏移量以从 VMA 转换到 LMA
    li      t1, 0xFFFFFFFFC0000000 - 0x80000000
    sub     t0, t0, t1
    srli    t0, t0, 12

    # 开启分页 (Sv39 模式)
    li      t1, 8 << 60
    or      t0, t0, t1
    csrw    satp, t0
    sfence.vma

    # 现在分页已开启，由于存在恒等映射，PC 依然有效。
    # 我们直接跳转到虚拟地址空间的 kern_init。
    la      sp, bootstacktop
    la      t0, kern_init
    jr      t0

.section .data
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:

.section .data
    .align PGSHIFT
    .global boot_page_table_sv39
boot_page_table_sv39:
    # 0x80000000 -> 0x80000000 (Index 2): 恒等映射
    .quad 0, 0
    .quad (0x80000 << 10) | 0xcf
    .zero 8 * (511 - 2)
    # 0xffffffffc0000000 -> 0x80000000 (Index 511): 高位映射
    .quad (0x80000 << 10) | 0xcf 

boot_hartid: .quad 0
boot_dtb:    .quad 0