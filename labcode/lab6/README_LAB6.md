# Lab 6 实验报告

## 任务完成情况

### 练习0：填写已有实验
本实验依赖实验2/3/4/5。已将实验2/3/4/5的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”“LAB5”的注释相应部分。
主要修改文件：
- `kern/process/proc.c`: 
    - `alloc_proc`: 初始化进程控制块。
    - `do_fork`: 创建子进程。
    - `proc_run`: **(修复)** 实现进程切换，包括 `lsatp` 和 `switch_to`。
    - `load_icode`: **(修复)** 设置用户态 trapframe，正确设置 `sstatus`。
- `kern/trap/trap.c`:
    - `trap_dispatch`: **(修复)** 在 `IRQ_S_TIMER` 中调用 `sched_class_proc_tick`，驱动调度器。
- `kern/mm/default_pmm.c`: 物理内存管理（已包含）。

### 练习1: 理解调度器框架的实现
已阅读并分析了调度器框架的相关代码。
- `sched_class` 结构体定义了调度器的接口。
- `run_queue` 结构体管理运行队列。
- `sched_init`, `wakeup_proc`, `schedule` 等函数实现了调度机制。

### 练习2: 实现 Round Robin 调度算法
完成了 `kern/schedule/default_sched.c` 中的 RR 调度算法实现。
- `RR_init`: 初始化运行队列。
- `RR_enqueue`: 将进程加入运行队列尾部，重置时间片。
- `RR_dequeue`: 将进程从运行队列移除。
- `RR_pick_next`: 选取运行队列头部的进程。
- `RR_proc_tick`: 减少当前进程时间片，如果时间片耗尽则标记需要调度。

验证方法：
默认情况下，ucore 使用 RR 调度器。编译运行 `make qemu`，如果系统能正常启动并运行用户程序（如 `matrix`），说明 RR 调度器工作正常。

### 扩展练习 Challenge 1: 实现 Stride Scheduling 调度算法
完成了 `kern/schedule/default_sched_stride.c` 中的 Stride 调度算法实现。
- `stride_init`: 初始化运行队列和斜堆。
- `stride_enqueue`: 将进程加入斜堆，更新时间片。
- `stride_dequeue`: 将进程从斜堆移除。
- `stride_pick_next`: 从斜堆中选取 stride 最小的进程，并更新其 stride 值。
- `stride_proc_tick`: 减少时间片，处理调度标记。
- 定义了 `BIG_STRIDE` 为 `0x7FFFFFFF`。

验证方法：
需要在 `kern/init/init.c` 中调用 `sched_init` 时，或者在 `kern/schedule/sched.c` 中修改默认调度器为 `stride_sched_class`。
可以通过运行 `priority` 测试程序来验证 Stride 调度器的正确性。

### 扩展练习 Challenge 2: 实现 FIFO 调度算法
创建了 `kern/schedule/default_sched_fifo.c` 文件，实现了 FIFO 调度算法。
- FIFO 算法是非抢占式的（或者说只有进程主动放弃或结束才调度，这里简单实现为不基于时间片轮转）。
- `FIFO_init`, `FIFO_enqueue`, `FIFO_dequeue`, `FIFO_pick_next` 与 RR 类似，但 `FIFO_proc_tick` 不做任何事（不减少时间片，不强制调度）。

验证方法：
同样需要修改调度器绑定，将默认调度器设置为 `fifo_sched_class`。

## 如何切换调度算法

在 ucore 中，调度算法通过 `sched_class` 接口进行抽象。要切换调度算法，主要有两种方式：

1.  **修改 `kern/schedule/sched.c` 中的 `sched_init` 函数**：
    在 `sched_init` 函数中，`sched_class` 指针被赋值为具体的调度算法实现。
    例如，要使用 Stride 调度器，可以将：
    ```c
    sched_class = &default_sched_class; // RR
    ```
    改为：
    ```c
    sched_class = &stride_sched_class; // Stride
    ```
    或者：
    ```c
    sched_class = &fifo_sched_class; // FIFO
    ```
    注意需要包含相应的头文件或声明外部变量。

2.  **在 `kern/schedule/default_sched.h` 中声明**：
    确保新的调度类（如 `stride_sched_class`, `fifo_sched_class`）在头文件中声明，以便在 `sched.c` 中使用。

## 验证逻辑

1.  **RR**: 默认配置。运行 `make qemu`，观察系统启动和 shell 交互。运行 `matrix` 等计算密集型程序，观察是否能并发执行。
2.  **Stride**: 修改 `sched_init` 使用 `stride_sched_class`。运行 `make qemu`。在 shell 中运行 `priority` 程序。`priority` 程序会创建不同优先级的子进程。如果 Stride 实现正确，高优先级的进程应该获得更多的 CPU 时间，输出结果中 stride 值应该符合预期。
3.  **FIFO**: 修改 `sched_init` 使用 `fifo_sched_class`。运行 `make qemu`。运行多个死循环或长任务程序，观察是否会发生“饥饿”现象，即一个进程一直运行，其他进程无法获得 CPU（除非该进程主动 yield 或退出）。

## 关键代码说明

### RR (Round Robin)
- 使用双向链表 `run_list` 管理就绪进程。
- `RR_enqueue` 使用 `list_add_before` 将进程加到队尾。
- `RR_pick_next` 使用 `list_next` 获取队头进程。
- `RR_proc_tick` 递减 `time_slice`，为 0 时设置 `need_resched`。

### Stride
- 使用斜堆（Skew Heap）`lab6_run_pool` 管理就绪进程，以 `stride` 值作为优先级（越小越优先）。
- `BIG_STRIDE` 设为 `0x7FFFFFFF` (最大正整数)，用于计算步长 `pass = BIG_STRIDE / priority`。
- `stride_pick_next` 每次选择堆顶元素（最小 stride），并更新其 stride：`p->lab6_stride += BIG_STRIDE / p->lab6_priority`。
- 注意处理 `priority` 为 0 的情况，避免除零错误。

### FIFO
- 类似于 RR，但 `proc_tick` 不强制调度。
- 进程一直运行直到结束或阻塞。

