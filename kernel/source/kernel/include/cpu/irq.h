/**
 * 手写操作系统
 *
 * 文件名: irq.h
 * 功  能: 中断处理程序
 */

#ifndef IRQ_H
#define IRQ_H


#include "comm/types.h"


/**
 * 以下IDT表项属性详细设置说明请参考Intel X86芯片手册
 * ../documents/intel编程文档/325384-sdm-vol-3abcd.pdf
 * Figure 6-2. IDT Gate Descriptors, Page208
 */

/**
 * 2. 异常/中断
 * 1) 异常
 *    由于CPU内部事件所引起的中断, 如: 非法指令, 地址越界, 除0异常等程序出错情况
 *    通常由于执行了现行指令所引起的
 * 2) 中断
 *    由于外部设备事件所引起的中断, 如: 如通常的磁盘中断, 打印机中断等硬件中断
 *    通常与现行指令无关, 右外部硬件中断事件引起
 *
 * 中断发生前程序一直在运行, 当触发中断后, CPU暂停当前运行的程序, 转而去处理中断事件
 * 1) 根据中断号判断是什么类型的中断
 * 2) 再去中断向量表中取出相应的中断服务程序地址
 * 3) 执行中断服务程序
 * 4) 执行完后返回被中断的正常程序继续执行中断发生前的程序
 *
 * 0-31操作系统固定的中断
 *
 * IDTR寄存器指向中断描述符表起始地址
 * [47,16] IDT Base Addres
 * [15,0]  IDT limit
 *
 * IDT - Interrupt Descriptor Table
 * Interrupt Gate
 * Task Gate
 * Trap Gate
 *
 * 当某一中断触发时的工作流程
 * 1) 通过IDTR寄存器得到IDT表基地址 IDT Base Addres, 找到内存中IDT表
 * 2) 根据中断向量号(即中断号)从IDT表中取得对应 Interrupt Gate Descriptor(64位)中断描述符表项
 * 3) 中断门描述符有三部分组成: offset偏移/attribute属性/Segment Selector段选择因子
 * 4) 通过 Segment Selector 段选择因子可从GDT或LDT表中取出对应 Segment Descriptor
 * 5) 最后将 中断门描述符中的offset偏移 + GDT或LDT表中的 Base Address, 可跳转到对应中断处理程序开始地址处执行
 */

// 常见中断号码宏定义
#define IRQ0_DE              0   // Divider Error exception
#define IRQ1_DB              1   // Debug Exception
#define IRQ2_NMI             2   // Non-Mask Interrupt
#define IRQ3_BP              3   // BreakPoint
#define IRQ4_OF              4   // OverFlow
#define IRQ5_BR              5   // Bound Range exceeded
#define IRQ6_UD              6   // UnDefined operation code
#define IRQ7_NM              7   // device Not available
#define IRQ8_DF              8   // Double Fault
#define IRQ10_TS             10  // invalid TSS(Task State Segment)
#define IRQ11_NP             11  // segment Not Present
#define IRQ12_SS             12  // Stack Segment fault
#define IRQ13_GP             13  // General Protection
#define IRQ14_PF             14  // Page Fault
#define IRQ16_MF             16
#define IRQ17_AC             17
#define IRQ18_MC             18
#define IRQ19_XM             19
#define IRQ20_VE             20

#define IRQ0_TIMER           0x20
#define IRQ1_KEYBOARD        0x21 // 按键中断
#define IRQ14_HARDDISK_PRIMARY   0x2E  // 主总线上的ATA磁盘中断

#define IRQ80_SYSCALL        0x80 // system call 系统调用中断

#define ERR_PAGE_P           (1 << 0)
#define ERR_PAGE_WR          (1 << 1)
#define ERR_PAGE_US          (1 << 1)

#define ERR_EXT              (1 << 0)
#define ERR_IDT              (1 << 1)


/**
 * 中断发生时相应的栈结构，暂时为无特权级发生的情况
 */
typedef struct _exception_frame_t
{
    // 结合压栈的过程，以及pusha指令的实际压入过程
    // 手动压栈的寄存器: ds, es, fs, gs
    uint32_t gs, fs, es, ds;
    // pushal指令自动压栈的寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t num;
    // 硬件会自动压栈的寄存器: eflags, cs, eip, error_code错误码(可选, 暂不处理)
    uint32_t error_code;
    uint32_t eip, cs, eflags;
    // 特权级3的esp和ss
    uint32_t esp3, ss3;
} exception_frame_t;


typedef void(*irq_handler_t)(void);
typedef uint32_t irq_state_t;

void irq_init(void);
int irq_install(int irq_num, irq_handler_t handler);

void exception_handler_unknown(void);
void exception_handler_divider(void);
void exception_handler_Debug(void);
void exception_handler_NMI(void);
void exception_handler_breakpoint(void);
void exception_handler_overflow(void);
void exception_handler_bound_range(void);
void exception_handler_invalid_opcode(void);
void exception_handler_device_unavailable(void);
void exception_handler_double_fault(void);
void exception_handler_invalid_tss(void);
void exception_handler_segment_not_present(void);
void exception_handler_stack_segment_fault(void);
void exception_handler_general_protection(void);
void exception_handler_page_fault(void);
void exception_handler_fpu_error(void);
void exception_handler_alignment_check(void);
void exception_handler_machine_check(void);
void exception_handler_smd_exception(void);
void exception_handler_virtual_exception(void);

void exception_handler_syscall_irq(void);


// PIC中断控制器8259A芯片相关的寄存器及位配置
#define PIC0_ICW1            0x20
#define PIC0_ICW2            0x21
#define PIC0_ICW3            0x21
#define PIC0_ICW4            0x21
#define PIC0_OCW2            0x20
#define PIC0_IMR             0x21

#define PIC1_ICW1            0xa0
#define PIC1_ICW2            0xa1
#define PIC1_ICW3            0xa1
#define PIC1_ICW4            0xa1
#define PIC1_OCW2            0xa0
#define PIC1_IMR             0xa1

#define PIC_ICW1_ICW4        (1 << 0)        // 1 - 需要初始化ICW4
#define PIC_ICW1_ALWAYS_1    (1 << 4)        // 总为1的位
#define PIC_ICW4_8086        (1 << 0)        // 8086工作模式

#define PIC_OCW2_EOI         (1 << 5)        // 1 - 非特殊结束中断EOI命令

#define IRQ_PIC_START        0x20            // PIC中断起始号


void irq_enable(int irq_num);
void irq_disable(int irq_num);
void irq_disable_global(void);
void irq_enable_global(void);

/**
 * 进入临界区方法只一: 关中断/开中断
 * 实现时在进入临界区前先读取CPU eflags寄存器的状态信息, 然后关中断
 * 等执行完临界区代码后, 在退出临界区后写回CPU eflags寄存器
 */
irq_state_t irq_enter_protection(void);
void irq_leave_protection(irq_state_t state);

void pic_send_eoi(int irq);


#endif
