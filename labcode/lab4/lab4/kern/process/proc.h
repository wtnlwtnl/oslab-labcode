//新增：实现进程、线程相关功能，包括：创建进程/线程，初始化进程/线程，处理进程/线程退出等功能
#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

struct context
{
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct {
    enum proc_state state;                  // 进程状态：RUNNABLE/SLEEPING/ZOMBIE/...；调度只选 RUNNABLE
    int pid;                                // 进程号：系统内唯一，用于定位/管理
    int runs;                               // 被调度运行的次数或时间片计数：便于统计/简单公平性
    uintptr_t kstack;                       // 该进程的内核栈起始虚拟地址（页对齐，栈向低地址生长）
    volatile bool need_resched;             // 置 1 表示需尽快调度让出 CPU（时钟中断/系统调用会设置）
    struct proc_struct *parent;             // 父进程指针：exit/wait 路径上用于回收/通知
    struct mm_struct *mm;                   // 进程的内存描述符：VMA/pgdir/映射信息（内核线程一般为 NULL）
    struct context context;                 // 上下文切换所需的最小寄存器集（switch_to 用，不是完整 trapframe）
    struct trapframe *tf;                   // 最近一次陷入内核时保存在内核栈上的完整寄存器现场（返回用户态用）
    uintptr_t pgdir;                        // 根页表（页目录）基地址（内核虚拟地址），用户进程等于 mm->pgdir
    uint32_t flags;                         // 进程标志位（位掩码）：退出/跟踪/COW 等控制信息（依实验定义）
    char name[PROC_NAME_LEN + 1];           // 进程名（结尾 '\0'），用于日志/调试
    list_entry_t list_link;                 // 双向链表节点：挂到全局进程表/就绪队列等
    list_entry_t hash_link;                 // 双向链表节点：挂到按 pid 的哈希桶，便于 O(1) 查找
};


#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
