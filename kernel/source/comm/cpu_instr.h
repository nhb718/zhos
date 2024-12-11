/**
 * 手写操作系统
 *
 * 文件名: cpu_instr.h
 * 功  能: 汇编指令的封装
 */

#ifndef CPU_INSTR_H
#define CPU_INSTR_H

#include "types.h"

// 读/写端口
static inline uint8_t inb(uint16_t port)
{
    uint8_t rval;
    /**
     * 以下intel X86汇编指令等效于: inb al, dx
     * gcc汇编指令: inb %[p], %[v]
     * 源,目的操作数位置正好相反
     */
    // inb %[p], %[v]  --- gcc的汇编语法, inb al, dx
    __asm__ __volatile__("inb %[p], %[v]" : [v]"=a"(rval) : [p]"d"(port)); // p - port, v - value
    return rval;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t rval;
    // in ax, dx
    __asm__ __volatile__("in %[p], %[v]" : [v]"=a"(rval) : [p]"d"(port)); // p - port, v - value
    return rval;
}

static inline void outb(uint16_t port, uint8_t data)
{
    /**
     * 以下intel X86汇编指令等效于: outb al, dx
     */
    // outb %[v], %[p]  --- gcc的汇编语法
    __asm__ __volatile__("outb %[v], %[p]" : : [p]"d"(port), [v]"a"(data)); // p - port, v - value
}

static inline void outw(uint16_t port, uint16_t data)
{
    __asm__ __volatile__("out %[v], %[p]" : : [p]"d" (port), [v]"a" (data));
}

/**
 * 开/关中断接口
 * 控制中断只能控制本地 CPU 的中断，无法控制其它 CPU 核心的中断
 */
static inline void cli(void)
{
    __asm__ __volatile__("cli");
}

static inline void sti(void)
{
    __asm__ __volatile__("sti");
}

// 加载GDT表, 使GDTR寄存器指向GDT表起始地址
static inline void lgdt(uint32_t start, uint32_t size)
{
    struct
    {
        uint16_t limit;      // 段长度
        uint16_t start15_0;  // 起始地址低16位
        uint16_t start31_16; // 起始地址高16位
    } gdt;

    gdt.start31_16 = (start >> 16) & 0xFFFF;
    gdt.start15_0  = start & 0xFFFF;
    gdt.limit      = size - 1;

    /**
     * lidt指令加载全局描述符表、内存地址, 用于初始化‌GDTR全局描述符表寄存器并加载GDT全局描述符表的基地址
     * 该寄存器包含了GDT表的起始地址start addr和界限limit. 通过lgdt指令, 系统可以切换不同的GDT, 从而实现对内存访问的控制
     */
    __asm__ __volatile__("lgdt %[g]"::[g]"m"(gdt));  // m - memory
}

/**
 * CR0寄存器(CR0, Control Register0)
 * [0] 实模式/保护模式位, 可控制其值让系统进入保护模式(实模式/保护模式切换)
 * [31] 用来开启分页机制
 */
static inline uint32_t read_cr0(void)
{
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %[v]":[v]"=r"(cr0));
    return cr0;
}

static inline void write_cr0(uint32_t val)
{
    __asm__ __volatile__("mov %[v], %%cr0"::[v]"r"(val));
}

// CR2寄存器保存异常产生时的错误码
static inline uint32_t read_cr2(void)
{
    uint32_t cr2;
    __asm__ __volatile__("mov %%cr2, %[v]":[v]"=r"(cr2));
    return cr2;
}

// CR3寄存器保存页目录基地址(用于分页机制)
static inline void write_cr3(uint32_t v)
{
    __asm__ __volatile__("mov %[v], %%cr3"::[v]"r"(v));
}

static inline uint32_t read_cr3(void)
{
    uint32_t cr3;
    __asm__ __volatile__("mov %%cr3, %[v]":[v]"=r"(cr3));
    return cr3;
}

// CR4寄存器保存 PSE, [4] 用于设置4KB/4MB的物理页(分页机制)
static inline uint32_t read_cr4(void)
{
    uint32_t cr4;
    __asm__ __volatile__("mov %%cr4, %[v]":[v]"=r"(cr4));
    return cr4;
}

static inline void write_cr4(uint32_t v)
{
    __asm__ __volatile__("mov %[v], %%cr4"::[v]"r"(v));
}

static inline void far_jump(uint32_t selector, uint32_t offset)
{
    uint32_t addr[] = {offset, selector};
    __asm__ __volatile__("ljmpl *(%[a])"::[a]"r"(addr));
}

// 加载IDT表, 使IDTR寄存器指向IDT表起始地址
static inline void lidt(uint32_t start, uint32_t size)
{
    struct
    {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    } idt;

    idt.start31_16 = (start >> 16) & 0xFFFF;
    idt.start15_0  = start & 0xFFFF;
    idt.limit      = size - 1;

    /**
     * lidt指令‌加载中断描述符表寄存器, 用于将源操作数中的值加载到中断描述符表格寄存器(IDTR)
     * 源操作数指定一个6字节的内存位置, 包含IDT中断描述符表格的基址(线性地址)与限制limit(表格大小，以字节计)
     */
    __asm__ __volatile__("lidt %0"::"m"(idt));
}

// CPU停止运行
static inline void hlt(void)
{
    __asm__ __volatile__("hlt");
}

// 写TR(Task Register)寄存器, 保存当前进程TSS段在GDT表中的段选择因子
static inline void write_tr(uint32_t tss_selector)
{
    __asm__ __volatile__("ltr %%ax"::"a"(tss_selector));
}

static inline uint32_t read_eflags(void)
{
    uint32_t eflags;

    // CPU eflags寄存器值不可直接读取, 只能通过将该寄存器值先压入栈中, 再从栈中弹出到eax寄存器的方式获得
    __asm__ __volatile__("pushfl\n\tpopl %%eax":"=a"(eflags));
    return eflags;
}

static inline void write_eflags(uint32_t eflags)
{
    // CPU eflags寄存器值不可直接写入, 需先将该值压入eax中, 然后再弹出给eflags寄存器
    __asm__ __volatile__("pushl %%eax\n\tpopfl"::"a"(eflags));
}


static inline void save_flags_cli(uint32_t * flags)
{
     __asm__ __volatile__(
            "pushfl \t\n"     // 把eflags寄存器压入当前栈顶
            "cli    \t\n"     // 关闭中断
            "popl %0 \t\n"    // 把当前栈顶弹出到flags为地址的内存中        
            : "=m"(*flags)
            :
            : "memory"
          );
}

static inline void restore_flags_sti(uint32_t * flags)
{
    __asm__ __volatile__(
              "pushl %0 \t\n" // 把flags为地址处的值寄存器压入当前栈顶
              "popfl \t\n"    // 把当前栈顶弹出到eflags寄存器中
              :
              : "m"(*flags)
              : "memory"
              );
}

#endif
