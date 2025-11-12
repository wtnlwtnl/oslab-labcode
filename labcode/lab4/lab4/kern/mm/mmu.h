//kern/mm/mmu.h       文件路径标识：说明这是内核内存管理相关头文件
#ifndef __KERN_MM_MMU_H__// 头文件保护宏开始：防止重复包含
#define __KERN_MM_MMU_H__// 定义头文件保护宏

#ifndef __ASSEMBLER__// 若当前不是在汇编阶段
#include <defs.h> // 引入通用内核类型/宏定义（如 uintptr_t 等）
#endif /* !__ASSEMBLER__ */ // 结束“非汇编”条件编译块

// A linear address 'la' has a three-part structure as follows:
// 说明：线性地址由四部分构成（两级目录+页表+页内偏移）
//
// +--------10------+-------10-------+---------12----------+    // 图：各字段位宽（9/9/9/12）
// | Page Directory |   Page Table   | Offset within Page  |    //图：字段名称（PDX1/PDX0/PTX/PGOFF）
// |      Index     |     Index      |                     |    //图：含义补充
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/     // 图：与宏的对应关系
//  \----------- PPN(la) -----------/                           // 图：高位合起来是 PPN(la)
//
// The PDX, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
// 注：PDX1/PDX0/PTX/PGOFF/PPN 这些宏按图示分解地址
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// 注：也提供了从索引和偏移重组地址的宏
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).
// 注：使用 PGADDR 组合出线性地址

// RISC-V uses 39-bit virtual address to access 56-bit physical address!     // 说明：Sv39 VA 39 位，可映射到最多 56 位物理地址
// Sv39 virtual address:                                                     // 图：Sv39 虚拟地址格式
// +----9----+----9---+----9---+---12--+                                     // 图：VPN[2]/VPN[1]/VPN[0]/PGOFF 位宽
// |  VPN[2] | VPN[1] | VPN[0] | PGOFF |                                     // 图：虚拟页号三级 + 页内偏移
// +---------+----+---+--------+-------+                                     // 图：结束
//                                                                           //
                                                                             // 图：Sv39 物理地址格式
// Sv39 physical address:                                                    // 说明：物理地址按 PPN[2:0] + PGOFF 组织
// +----26---+----9---+----9---+---12--+                                     // 图：PPN[2]/PPN[1]/PPN[0]/PGOFF 位宽
// |  PPN[2] | PPN[1] | PPN[0] | PGOFF |                                     // 图：物理页号三级 + 页内偏移
// +---------+----+---+--------+-------+                                     // 图：结束
//                                                                           //
                                                                             // 图：Sv39 页表项格式
// Sv39 page table entry:                                                    // 说明：PTE 存 PPN 与标志位（低 10 位为标志）
// +----26---+----9---+----9---+---2----+-------8-------+                    // 图：PPN[2]/PPN[1]/PPN[0]/Reserved/Flag
// |  PPN[2] | PPN[1] | PPN[0] |Reserved|D|A|G|U|X|W|R|V|                    // 图：标志位包括 V/R/W/X/U/G/A/D 等
// +---------+----+---+--------+--------+---------------+                    // 图：结束

// page directory index                                                      // 说明：从线性地址提取页目录索引
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)                  // 从 la 右移 PDX1SHIFT 后取低 9 位（VPN[2]）
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)                  // 从 la 右移 PDX0SHIFT 后取低 9 位（VPN[1]）

// page table index                                                          // 说明：从线性地址提取页表索引
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)                    // 从 la 右移 PTXSHIFT 后取低 9 位（VPN[0]）

// page number field of address                                              // 说明：从线性地址提取“虚拟页号 PPN(la)”（三段 VPN 合并）
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)                               // 直接去掉页内偏移（低 12 位）后得到虚拟页号

// offset in page                                                            // 说明：从线性地址提取页内偏移
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)                                 // 取 la 的低 12 位作为页内偏移

// construct linear address from indexes and offset                          // 说明：用三级索引与偏移拼出线性地址
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT | (d0) << PDX0SHIFT | (t) << PTXSHIFT | (o))) // 组装：各字段左移到位后或起来

// address in page table or page directory entry                             // 说明：从 PTE/PDE 中取出物理页基址（清标志并对齐成 PA）
// 把页表项里存储的地址拿出来                                                // 额外中文提示：强调语义同上
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT)) // 清低 10 位标志，再左移 2 位把 PPN 对齐到物理地址位
#define PDE_ADDR(pde)   PTE_ADDR(pde)                                         // 目录项与表项同格式，复用 PTE_ADDR 取得物理地址

/* page directory and page table constants */
#define NPDEENTRY       512                    // page directory entries per page directory
#define NPTEENTRY       512                    // page table entries per page table

#define PGSIZE          4096                    // bytes mapped by a page                   // 一页大小 4KB
#define PGSHIFT         12                      // log2(PGSIZE)                            // 4KB 的对数位移 12
#define PTSIZE          (PGSIZE * NPTEENTRY)    // bytes mapped by a page directory entry  // 一个上层表项覆盖 512×4KB=2MB
#define PTSHIFT         21                      // log2(PTSIZE)                            // 2MB 的对数位移 21

#define PTXSHIFT        12                      // offset of PTX in a linear address       // 线性地址中 PTX（VPN[0]）起始位偏移 12
#define PDX0SHIFT       21                      // offset of PDX0 in a linear address      // 线性地址中 PDX0（VPN[1]）起始位偏移 21
#define PDX1SHIFT       30                      // offset of PDX0 in a linear address      // 【笔误】应为“PDX1”；VPN[2] 起始位偏移 30
#define PTE_PPN_SHIFT   10                      // offset of PPN in a physical address     // PTE 中 PPN 从 bit10 开始（低 10 位为标志）

// page table entry (PTE) fields                            // 说明：PTE 各标志位常量（低 10 位）
#define PTE_V     0x001 // Valid                             // 有效位：条目是否可用；0 表示无效（访问会触发异常）
#define PTE_R     0x002 // Read                              // 读权限：页是否允许读（对叶子 PTE 生效）
#define PTE_W     0x004 // Write                             // 写权限：页是否允许写（对叶子 PTE 生效；W=1 & R=0 属保留组合，通常视为非法）
#define PTE_X     0x008 // Execute                           // 执行权限：页是否允许取指（支持 execute-only：X=1 可不必 R=1）
#define PTE_U     0x010 // User                              // 用户位：1 表示用户态可访问；0 表示仅特权态可用（受 SSTATUS.SUM 等影响）
#define PTE_G     0x020 // Global                            // 全局位：1 表示跨 ASID 全局映射（切换地址空间时可不必 TLB 逐项失效）
#define PTE_A     0x040 // Accessed                          // 访问位：硬件在首次读/写/取指时置 1（若硬件不支持，会以异常协助软件置位）
#define PTE_D     0x080 // Dirty                             // 脏位：硬件在写入时置 1（若硬件不支持，会以异常协助软件置位）
#define PTE_SOFT  0x300 // Reserved for Software             // RSW[1:0]（bit8..9）：留给软件自定义（如换出、COW 标记等）

#define PAGE_TABLE_DIR (PTE_V)                               // 仅置 V：常用于“非叶子（指向下级页表）条目”的最小合法标志
#define READ_ONLY (PTE_R | PTE_V)                            // 只读可用：有效 + 读
#define READ_WRITE (PTE_R | PTE_W | PTE_V)                   // 读写可用：有效 + 读 + 写（注意：W=1 通常意味着 R=1，一起置）
#define EXEC_ONLY (PTE_X | PTE_V)                            // 仅可执行：有效 + 执行（允许 execute-only）
#define READ_EXEC (PTE_R | PTE_X | PTE_V)                    // 可读且可执行：有效 + 读 + 执行
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V)      // 全权限：有效 + 读写 + 执行

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)     // 用户页常用：用户可读/写/执行 + 有效

#endif /* !__KERN_MM_MMU_H__ */
