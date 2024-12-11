/**
 * 手写操作系统
 *
 * 文件名: cpu.h
 * 功  能: 与x86的体系结构相关的接口及参数
 */

#ifndef CPU_H
#define CPU_H

#include "comm/types.h"


#define SEG_G               (1 << 15)       // 设置段界限limit字段的单位，1-4KB units，0-字节, limit字段20bits * 4KB可表示4GB
#define SEG_D               (1 << 14)       // 控制是否是32位、16位的代码段/数据段或栈段, 1-32位, 0-16位
#define SEG_P_PRESENT       (1 << 7)        // 指示段描述符在GDT表中是否存在, present=1代表Segment是存在的


/**
 * CPL 对象的特权属性(固有的)
 * RPL 操作的权限值
 * DPL 当前被操作对象的特权属性(固有的)
 * 
 * 使用者  操作权限   被操作对象
 * CPL    --RPL-->  DPL
 */
/* CPL - Current Privilege Level */
#define SEG_CPL0            (0 << 0)        // 当前特权级0, 最高特权级, 内核态
#define SEG_CPL3            (3 << 0)        // 当前特权级3, 最低权限, 用户态

/* DPL - Descriptor Privilege Level */
#define SEG_DPL0            (0 << 5)        // 被操作对象描述符的特权级0, 最高特权级, 内核态
#define SEG_DPL3            (3 << 5)        // 被操作对象描述符的特权级3, 最低权限, 用户态

/* RPL - Request Privilege Level */
#define SEG_RPL0            (0 << 0)        // 请求操作的特权级0, 最高特权级, 内核态
#define SEG_RPL3            (3 << 0)        // 请求操作的特权级3, 最低权限, 用户态

#define SEG_S_SYSTEM        (0 << 4)        // 是否是系统段, 如调用门或者中断, 0-系统段
#define SEG_S_NORMAL        (1 << 4)        // 普通的代码段或数据段, 1-代码段或数据段

#define SEG_TYPE_CODE       (1 << 3)        // 指定其为代码段和只读权限
#define SEG_TYPE_DATA       (0 << 3)        // 指定其为数据段和读写权限
#define SEG_TYPE_TSS      	(9 << 0)        // TSS任务状态段
#define SEG_TYPE_RW         (1 << 1)        // 对于数据段来说, 是否可写可读，不设置为只读; 对于代码段来说, 是否可读可执行, 不设置为只执行

// IDT表中有三种类型的门: 中断门IDT/陷阱门Trap/系统调用门Syscall, 通过属性中type字段不同来区分
#define GATE_TYPE_IDT       (0xE << 8)      // 32位中断门描述符, 0xE为中断门 Interrupt Gate
#define GATE_TYPE_SYSCALL   (0xC << 8)      // 0xC为系统调用门 Task Gate
//#define GATE_TYPE_TRAP      (0xF << 8)      // 0xF为陷阱门 Trap Gate
#define GATE_P_PRESENT      (1 << 15)       // 是否存在, present =1, gate存在的
#define GATE_DPL0           (0 << 13)       // 特权级0，最高特权级, 内核态
#define GATE_DPL3           (3 << 13)       // 特权级3，最低权限, 用户态

#define EFLAGS_IF           (1 << 9)       // CPU中断控制位, 1-开中断, 0-屏蔽中断
#define EFLAGS_DEFAULT      (1 << 1)       // 固定为1


#pragma pack(1) // 1字节对齐


/**
 * 以下GDT表项属性详细设置说明请参考Intel X86芯片手册
 * ../documents/intel编程文档/325384-sdm-vol-3abcd.pdf
 * Figure 3-8. Segment Descriptor, Page340
 */


/**
 * GDT - Global Descriptor Table
 * TSS - Task State Segment
 * GDT表中存储的数据如下:
 * Seg desc  -->  Code/Data/Stack Segment 代码段/数据段/栈段
 * TSS desc  -->  Task State Segment 任务状态段
 * Seg desc
 * TSS desc
 * LDT desc  -->  Local Descriptor Table 局部描述符表
 *
 * Seg desc指向 Code, Data or Stack Segment
 *         也可指向Interrupt Handler
 * TSS desc指向进程状态寄存器
 *
 * descriptor一共64字节, 包含3大部分: base addr基地址, limit界限 和 属性值
 * 
 * base addr[31:24]   G[23] D/B[22] L[21] AVL[20]   Seg limit[19:16]   P[15] DPL[14:13] S[12] Type[11:8]   base addr[23:16]
 * base addr[15:0]   Seg limit[15:0]
 *
 * L    - 64-bits code segment(IA32-e mode only) 64位操作系统
 *        =1系统运行在64位模式, =0系统运行在32位模式
 * AVL  - Available for use by system software
 * base - Segment base address
 * D/B  - Default opeartion size(0=16bits segment, 1=32bit segment)
 *        1=32位段数据, 0=16位段数据
 * DPL  - Descruptor Privilege level
 * G    - Granularity
 *        =1, limit的值 = limit(20-bits) * 4KB = 32bits
 * limit- Segment limit
 * P    - Segment present
 *        段是否存在的标志位, 1=存在, 0=不存在
 * S    - Descriptor type(0=system, 1=code or data)
 * type - Segment type
 */
typedef struct _segment_desc_t
{
    uint16_t limit15_0;
    uint16_t base15_0;
    uint8_t base23_16;
    uint16_t attr;
    uint8_t base31_24;
} segment_desc_t;

/*
 * IDT段描述符结构体
 */
typedef struct _gate_desc_t
{
    uint16_t offset15_0;
    uint16_t selector;
    uint16_t attr;
    uint16_t offset31_16;
} gate_desc_t;

/**
 * TSS描述符结构体
 * TSS - Task State Segment
 * 程序的运行状态, 包含了诸多信息
 * 当前正在执行哪些代码, 有哪些数据, 使用了哪块区域做堆栈, 当前执行指令的地址
 * 前一指令的运行状态, 运算所用的寄存器信息等
 * 1) 程序指令.text, 常量.rodata, 数据.data .bss
 * 2) 栈相关SS, ESP
 * 3) 通用寄存器EAX..EDI, 状态寄存器EFLAGS, 当前运行地址EIP
 *
 * 因此当操作系统让CPU在多个进程之间切换支持代码时, 核心工作就是保存上一程序的运行状态到自己的TSS中
 * 再加载下一待运行程序的TSS中的值到相应的位置
 *
 * 参考资料
 * ../documents/325384-sdm-vol-3abcd.pdf  Chapter 7.2 Task Management Data Structures
 */
typedef struct _tss_t
{
    uint32_t pre_link; // 前一个任务链接, 此项目暂未用到
    // ss0,esp0, ss1,esp1, ss2,esp2 保护模式下特权级程序使用的栈相关寄存器, 我们只用到ss0, 对应特权级0
    uint32_t esp0, ss0, esp1, ss1, esp2, ss2; 
    uint32_t cr3; // 保存虚拟内存页目录项首地址
    uint32_t eip, eflags; // CPU运行状态寄存器
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; // 通用寄存器
    uint32_t es, cs, ss, ds, fs, gs; // 段寄存器
    uint32_t ldt; // LDT Segment Selector, 此项目暂未用到
    uint32_t iomap; // IO map base address, IO位图, 此项目暂未用到
    //uint32_t ssp; // 此项目暂未用到
} tss_t;
#pragma pack()


void cpu_init(void);
void segment_desc_set(uint16_t selector, uint32_t base, uint32_t limit, uint16_t attr);
void gate_desc_set(gate_desc_t * desc, uint16_t selector, uint32_t offset, uint16_t attr);
int gdt_alloc_desc(void);
void gdt_free_sel(int sel);

void switch_to_tss(uint32_t tss_selector);


#endif

