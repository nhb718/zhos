/**
 * 手写操作系统
 *
 * 文件名: irq.c
 * 功  能: 中断处理
 */

#include "cpu/irq.h"
#include "cpu/cpu.h"
#include "comm/cpu_instr.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "core/task.h"
#include "core/syscall.h"


#define IDT_TABLE_NR            256           // IDT表项数量, X86支持256个中断向量

static gate_desc_t idt_table[IDT_TABLE_NR];   // 中断描述符表



/**
 * 计算机中可能有很多设备, 每个设备都可能产生中断, 但是中断控制器的中断信号线是有限的
 * 这里没有实现中断控制器的 "多个设备共享一根中断信号线" 的功能
 */

static void dump_core_regs(exception_frame_t * frame)
{
    // 打印CPU寄存器相关内容
    uint32_t esp, ss;
    if (frame->cs & 0x7) // 取得当前进程的代码段低3位的值>0, 说明特权级为3的进程(用户态进程)
    {
        ss = frame->ds;
        esp = frame->esp;
    }
    else
    {
        ss = frame->ss3;
        esp = frame->esp3;
    }

    log_printf("IRQ: %d, error code: %d.", frame->num, frame->error_code);
    log_printf("CS: %d\nDS: %d\nES: %d\nSS: %d\nFS:%d\nGS:%d",
               frame->cs, frame->ds, frame->es, ss, frame->fs, frame->gs);
     log_printf("EAX:0x%x\n"
                "EBX:0x%x\n"
                "ECX:0x%x\n"
                "EDX:0x%x\n"
                "EDI:0x%x\n"
                "ESI:0x%x\n"
                "EBP:0x%x\n"
                "ESP:0x%x\n",
               frame->eax, frame->ebx, frame->ecx, frame->edx,
               frame->edi, frame->esi, frame->ebp, esp);
    log_printf("EIP:0x%x\nEFLAGS:0x%x\n", frame->eip, frame->eflags);
}

static void do_default_handler(exception_frame_t * frame, const char * message)
{
    log_printf("--------------------------------");
    log_printf("IRQ/Exception happend: %s.", message);
    dump_core_regs(frame);

    // todo: 留等以后补充打印任务栈的内容

    log_printf("--------------------------------");
    if (frame->cs & 0x3)
    {
        sys_exit(frame->error_code);
    }
    else
    {
        for (;;)
        {
            hlt(); // CPU进入停止运行状态
        }
    }
}

void do_handler_unknown(exception_frame_t * frame)
{
    do_default_handler(frame, "Unknown exception.");
}

void do_handler_divider(exception_frame_t * frame)
{
    do_default_handler(frame, "Divider Error.");
}

void do_handler_Debug(exception_frame_t * frame)
{
    do_default_handler(frame, "Debug Exception");
}

void do_handler_NMI(exception_frame_t * frame)
{
    do_default_handler(frame, "NMI Interrupt.");
}

void do_handler_breakpoint(exception_frame_t * frame)
{
    do_default_handler(frame, "Breakpoint.");
}

void do_handler_overflow(exception_frame_t * frame)
{
    do_default_handler(frame, "Overflow.");
}

void do_handler_bound_range(exception_frame_t * frame)
{
    do_default_handler(frame, "BOUND Range Exceeded.");
}

void do_handler_invalid_opcode(exception_frame_t * frame)
{
    do_default_handler(frame, "Invalid Opcode.");
}

void do_handler_device_unavailable(exception_frame_t * frame)
{
    do_default_handler(frame, "Device Not Available.");
}

void do_handler_double_fault(exception_frame_t * frame)
{
    do_default_handler(frame, "Double Fault.");
}

void do_handler_invalid_tss(exception_frame_t * frame)
{
    do_default_handler(frame, "Invalid TSS");
}

void do_handler_segment_not_present(exception_frame_t * frame)
{
    do_default_handler(frame, "Segment Not Present.");
}

void do_handler_stack_segment_fault(exception_frame_t * frame)
{
    do_default_handler(frame, "Stack-Segment Fault.");
}

void do_handler_general_protection(exception_frame_t * frame)
{
    log_printf("--------------------------------");
    log_printf("IRQ/Exception happend: General Protection.");
    if (frame->error_code & ERR_EXT)
    {
        log_printf("the exception occurred during delivery of an "
                "event external to the program, such as an interrupt"
                "or an earlier exception.");
    }
    else
    {
        log_printf("the exception occurred during delivery of a"
                    "software interrupt (INT n, INT3, or INTO).");
    }

    if (frame->error_code & ERR_IDT)
    {
        log_printf("the index portion of the error code refers "
                    "to a gate descriptor in the IDT");
    }
    else
    {
        log_printf("the index refers to a descriptor in the GDT");
    }

    /**
     * 打印段选择因子
     * [3:16] segment selector index
     * [2] TI
     * [1] IDT
     * [0] EXT
     */
    log_printf("segment selector index: %d", frame->error_code & 0xFFF8);

    dump_core_regs(frame);
    if (frame->cs & 0x3)
    {
        sys_exit(frame->error_code);
    }
    else
    {
        for (;;)
        {
            hlt(); // CPU进入停止运行状态
        }
    }
}

/**
 * [0] P - Present, 0: non-present page, 1: page-level protection violation
 * [1] W/R - 0: access causing by a read, 1: access causing by a write
 * [2] U/S - 0: supervisor-mode access fault, 1: user-mode access fault
 */
void do_handler_page_fault(exception_frame_t * frame)
{
    log_printf("--------------------------------");
    log_printf("IRQ/Exception happend: Page fault.");
    uint32_t err_code = read_cr2();
    // 访问的页权限异常(分页机制), TBD!!! 写保护异常暂未实现, 这是fork子进程时父子进程共享父进程数据区的机制
    if (frame->error_code & ERR_PAGE_P)
    {
        log_printf("\tpage-level protection violation: 0x%x.", err_code);
    }
    // 访问的页不存在异常(分页机制), TBD!!! 缺页异常暂未实现
    else
    {
         log_printf("\tPage doesn't present 0x%x", err_code);
    }

    // 读或执行 操作产生的异常
    if (frame->error_code & ERR_PAGE_WR)
    {
        log_printf("\tThe access causing the fault was a read. 0x%x", err_code);
    }
    // 写操作产生的异常
    else
    {
        log_printf("\tThe access causing the fault was a write. 0x%x", err_code);
    }

    // 内核态进程产生的异常
    if (frame->error_code & ERR_PAGE_US)
    {
        log_printf("\tA supervisor-mode access caused the fault. 0x%x", err_code);
    }
    // 用户态进程产生的异常
    else
    {
        log_printf("\tA user-mode access caused the fault. 0x%x", err_code);
    }

    dump_core_regs(frame);
    if (frame->cs & 0x3)
    {
        sys_exit(frame->error_code);
    }
    else
    {
        for (;;)
        {
            hlt(); // CPU进入停止运行状态
        }
    }

}

void do_handler_fpu_error(exception_frame_t * frame)
{
    do_default_handler(frame, "X87 FPU Floating Point Error.");
}

void do_handler_alignment_check(exception_frame_t * frame)
{
    do_default_handler(frame, "Alignment Check.");
}

void do_handler_machine_check(exception_frame_t * frame)
{
    do_default_handler(frame, "Machine Check.");
}

void do_handler_smd_exception(exception_frame_t * frame)
{
    do_default_handler(frame, "SIMD Floating Point Exception.");
}

void do_handler_virtual_exception(exception_frame_t * frame)
{
    do_default_handler(frame, "Virtualization Exception.");
}


/**
 * 根据8259A中断控制器的芯片手册配置相关寄存器
 * 8259A芯片的参考资料:
 * https://wiki.osdev.org/8259_PIC
 * ../8259A.pdf        
 *   
 * 相关书籍:
 * 《Linux内核完全剖析  第180页》
 */
static void init_pic(void)
{
    // 中断控制器 8259A Master 配置
    // 边缘触发，级联，需要配置icw4, 8086模式
    outb(PIC0_ICW1, PIC_ICW1_ALWAYS_1 | PIC_ICW1_ICW4);
    // 对应的中断号起始序号0x20
    outb(PIC0_ICW2, IRQ_PIC_START);
    // 主片IRQ2有从片
    outb(PIC0_ICW3, 1 << 2);
    // 普通全嵌套、非缓冲、非自动结束、8086模式
    outb(PIC0_ICW4, PIC_ICW4_8086);

    // 中断控制器 8259A Slave 配置, 通过 Master上2号引脚相连
    // 边缘触发，级联，需要配置icw4, 8086模式
    outb(PIC1_ICW1, PIC_ICW1_ICW4 | PIC_ICW1_ALWAYS_1);
    // 起始中断序号，要加上8
    outb(PIC1_ICW2, IRQ_PIC_START + 8);  // 1块芯片上有8个引脚
    // 没有从片，连接到主片的IRQ2上
    outb(PIC1_ICW3, 2);
    // 普通全嵌套、非缓冲、非自动结束、8086模式
    outb(PIC1_ICW4, PIC_ICW4_8086);

    // 屏蔽所有中断, 两芯片相连的2号引脚永远开启, 允许从PIC1传来的中断
    outb(PIC0_IMR, 0xFF & ~(1 << 2));   // 2号引脚是链接slave从芯片的, 因此需默认打开
    outb(PIC1_IMR, 0xFF);
}

void pic_send_eoi(int irq_num)
{
    irq_num -= IRQ_PIC_START;

    // 从片也可能需要发送EOI
    if (irq_num >= 8) // 8259A芯片Slave发出的
    {
        outb(PIC1_OCW2, PIC_OCW2_EOI);
    }
    outb(PIC0_OCW2, PIC_OCW2_EOI);
}

/**
 * @brief 中断和异常初始化, 设置整张中断表
 */
void irq_init(void)
{
    // 先将所有IDT表项全清0
    for (uint32_t i = 0; i < IDT_TABLE_NR; i++)
    {
        // 设置所有中断/异常的默认处理函数
        gate_desc_set(idt_table + i, KERNEL_SELECTOR_CS, (uint32_t) exception_handler_unknown,
                      GATE_P_PRESENT | GATE_DPL0 | GATE_TYPE_IDT);
    }

    // 设置异常处理接口, 注册所有异常和中断的处理函数
    irq_install(IRQ0_DE, exception_handler_divider);
    irq_install(IRQ1_DB, exception_handler_Debug);
    irq_install(IRQ2_NMI, exception_handler_NMI);
    irq_install(IRQ3_BP, exception_handler_breakpoint);
    irq_install(IRQ4_OF, exception_handler_overflow);
    irq_install(IRQ5_BR, exception_handler_bound_range);
    irq_install(IRQ6_UD, exception_handler_invalid_opcode);
    irq_install(IRQ7_NM, exception_handler_device_unavailable);
    irq_install(IRQ8_DF, exception_handler_double_fault);
    irq_install(IRQ10_TS, exception_handler_invalid_tss);
    irq_install(IRQ11_NP, exception_handler_segment_not_present);
    irq_install(IRQ12_SS, exception_handler_stack_segment_fault);
    irq_install(IRQ13_GP, exception_handler_general_protection);
    irq_install(IRQ14_PF, exception_handler_page_fault);
    irq_install(IRQ16_MF, exception_handler_fpu_error);
    irq_install(IRQ17_AC, exception_handler_alignment_check);
    irq_install(IRQ18_MC, exception_handler_machine_check);
    irq_install(IRQ19_XM, exception_handler_smd_exception);
    irq_install(IRQ20_VE, exception_handler_virtual_exception);

    //irq_install(IRQ80_SYSCALL, exception_handler_syscall_irq); // 该接口设置的权限时DPL0
    // 此处需要设置int $0x80 的DPL3权限, 以允许应用层调用系统调用接口
    gate_desc_set(idt_table + IRQ80_SYSCALL, KERNEL_SELECTOR_CS, (uint32_t)exception_handler_syscall_irq,
                  GATE_P_PRESENT | GATE_DPL3 | GATE_TYPE_IDT);

    // 加载IDT表, 使IDTR寄存器指向IDT表起始地址
    lidt((uint32_t)idt_table, sizeof(idt_table));

    // 初始化 PIC中断控制器(Programmable Interrupt Controller)
    init_pic();
}


/**
 * @brief 在IDT表中初始化对应中断号的中断/异常处理程序的中断描述符
 */
int irq_install(int irq_num, irq_handler_t handler)
{
    if (irq_num >= IDT_TABLE_NR)
    {
        return -1;
    }

    /**
     * idt_table + irq_num -  idt_table基地址 + 中断号
     * KERNEL_SELECTOR_CS - 内核代码段起始地址
     * handler - 对应的中断处理程序
     * GATE_P_PRESENT | GATE_DPL0 | GATE_TYPE_IDT - 中断属性
     */
    // IDTR: idt_table base + irq_num => idt descriptor(selector + offset)
    // selector(kernel_CS) => gdt_table +selector  => gdt descriptor(base)
    // base + offset
    gate_desc_set(idt_table + irq_num, KERNEL_SELECTOR_CS, (uint32_t) handler,
                  GATE_P_PRESENT | GATE_DPL0 | GATE_TYPE_IDT);
    return 0;
}

// 开启/屏蔽对应中断号的中断, 需设置8259A芯片对应中断号的引脚位清0/置1
void irq_enable(int irq_num)
{
    if (irq_num < IRQ_PIC_START)
    {
        return;
    }

    // 先将中断号与0对齐, 方便对应中断控制器上引脚号
    irq_num -= IRQ_PIC_START;
    if (irq_num < 8) // 对齐后的中断号<8, 说明第一个块8259A芯片触发的中断
    {
        uint8_t mask = inb(PIC0_IMR) & ~(1 << irq_num); // 读出IMR0寄存器, 将中断号对应的位清0
        outb(PIC0_IMR, mask); // 写回IMR0寄存器
    }
    else // 对齐后的中断号>=8, 说明第二块8259A芯片触发的中断
    {
        irq_num -= 8; // 因为引脚编号都是0~7, 因此当irq_num>8时需要减8
        uint8_t mask = inb(PIC1_IMR) & ~(1 << irq_num); // 读出IMR1寄存器, 将中断号对应的位清0
        outb(PIC1_IMR, mask); // 写回IMR1寄存器
    }
}

void irq_disable(int irq_num)
{
    if (irq_num < IRQ_PIC_START)
    {
        return;
    }

    // 先将中断号与0对齐, 方便对应中断控制器上引脚号
    irq_num -= IRQ_PIC_START;
    if (irq_num < 8) // 对齐后的中断号<8, 说明第一个块8259A芯片触发的中断
    {
        uint8_t mask = inb(PIC0_IMR) | (1 << irq_num); // 读出IMR0寄存器, 将中断号对应的位设为1
        outb(PIC0_IMR, mask); // 写回IMR0寄存器
    }
    else
    {
        irq_num -= 8;
        uint8_t mask = inb(PIC1_IMR) | (1 << irq_num); // 读出IMR1寄存器, 将中断号对应的位设为1
        outb(PIC1_IMR, mask); // 写回IMR1寄存器
    }
}

// 开启/屏蔽所有中断, 只需设置CPU上IF(Interrput Flag)寄存器
void irq_disable_global(void)
{
    cli();
}

void irq_enable_global(void)
{
    sti();
}

/**
 * @brief 进入中断保护, 读出当前CPU eflags寄存器状态信息, 然后再关中断
 */
irq_state_t irq_enter_protection(void)
{
    irq_state_t state = read_eflags(); // 读出CPU eflags寄存器状态信息
    irq_disable_global(); // 关中断, 其实是修改了eflags寄存器的[9] bit位 - Interrupt Enable Flag
    return state;
}

/**
 * @brief 退出中断保护, 将之前保存的CPU eflags寄存器状态信息写回CPU
 */
void irq_leave_protection(irq_state_t state)
{
    write_eflags(state); // 将之前读取的eflags寄存器状态信息写回CPU, 这样不会误开中断
}

