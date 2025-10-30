#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void) {
    extern char edata[], end[];
    // 先清零 BSS，再读取并保存 DTB 的内存信息，避免被清零覆盖（为了解释变化 正式上传时我觉得应该删去这句话）
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // 初始化控制台（串口/终端）：后续 cprintf/cputs 才能把信息打印到你的 QEMU/Spike 控制台
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);// 实际打印启动信息；比 cprintf 轻量，确保在早期初始化阶段能稳定输出

    print_kerninfo();// 打印内核关键信息：段地址、内核大小、编译信息等，便于你核对链接布局和调试

    // grade_backtrace();
    idt_init();  // init interrupt descriptor table
    // “初始化IDT”：在 RISC-V 上实质是：stvec = &__alltraps；sscratch=0（约定）
    // 必须先设置入口，否则中断一来就会“跑飞”

    pmm_init();  // init physical memory management

    idt_init();  // init interrupt descriptor table
     // 再次设置 trap 入口（多余但无害）：保证任何被覆盖/重定位后的 stvec 仍指向 __alltraps
     // 课程代码里常出现这种“保险式重复设置”，你可保留或删除（确认无副作用）

    clock_init();   // init clock interrupt
    // 初始化时钟：sie开STIE分路，计算/设置timebase，sbi_set_timer预约“第一次闹钟”
    // 没有这一步，“tick心跳”就不会开始，后面的时钟中断逻辑也就没法验证

    intr_enable();  // enable irq interrupt
    // 开总闸：sstatus.SIE=1（允许S态处理已使能类别的中断）
    // 注意：只有"分路(sie)"和"总闸(SIE)"都开了，且已经预约了定时器，时钟中断才会真正到达

    // 测试异常处理 - Challenge3
    cprintf("Testing exception handlers...\n");

    // 测试断点异常
    cprintf("Testing breakpoint exception:\n");
    asm volatile("ebreak");

    // 测试非法指令异常 - 使用一个确定的32位非法指令编码
    cprintf("Testing illegal instruction exception:\n");
    asm volatile(".word 0xffffffff");  // 32位非法指令（最低2位为11，表示32位指令）

    cprintf("Exception tests completed!\n");

    /* do nothing */
    while (1)
        ;
}

void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

void grade_backtrace(void) { grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000); }