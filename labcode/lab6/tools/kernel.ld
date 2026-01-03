OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

SECTIONS
{
    /* 1. 将入口代码强制链接在物理地址 0x80200000，确保 OpenSBI 能识别并跳转 */
    . = 0x80200000;
    .text.kern_entry : {
        *(.text.kern_entry)
    }

    /* 2. 从这里开始，内核切换到虚拟地址空间 */
    . = 0xFFFFFFFFC0200000 + SIZEOF(.text.kern_entry);

    .text : AT(0x80200000 + SIZEOF(.text.kern_entry)) {
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .);

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
        PROVIDE(edata = .);
    }

    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    }

    . = ALIGN(0x1000);
    PROVIDE(end = .);
}