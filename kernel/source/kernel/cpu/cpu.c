/**
 * 手写操作系统
 *
 * 文件名: cpu.c
 * 功  能: CPU设置
 */

#include "cpu/cpu.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "os_cfg.h"
#include "ipc/mutex.h"
#include "core/syscall.h"


static segment_desc_t gdt_table[GDT_TABLE_SIZE];
static mutex_t mutex;


/**
 * 设置段描述符
 * uint16_t selector  16位段选择因子, 即GDT表项在整个GDT表中偏移量, 取值范围[1,255] 因为其中第0个表项是CPU保留项
 * uint32_t base      GDT表项对应段的基地址
 * uint32_t limit     段的最大地址范围
 * uint16_t attr      段描述符的属性
 */
void segment_desc_set(uint16_t selector, uint32_t base, uint32_t limit, uint16_t attr)
{
    /**
     * selector是段选择因子索引号, 每个段描述符长度是8字节
     * 因此selector>>3 其实就是 selector / sizeof(segment_desc_t) = selector/8, 即selector>>3, 
     */
    segment_desc_t * desc = gdt_table + (selector >> 3);

    // 如果界限字段limit比较长, 将长度单位换成4KB, limit占用20bit * 4KB = 4GB
    if (limit > 0xFFFFF)
    {
        attr |= 0x8000;  // attr属性的G位设为1
        limit /= 0x1000; // limit以4KB为单位
    }

    desc->limit15_0   = limit & 0xffff;
    desc->base15_0    = base & 0xffff;
    desc->base23_16   = (base >> 16) & 0xff;
    desc->base31_24   = (base >> 24) & 0xff;
    desc->attr        = attr | (((limit >> 16) & 0xf) << 8);
}

/**
 * 设置门描述符
 * gate_desc_t * desc  64位门描述符指针
 * uint16_t selector   段选择因子
 */
void gate_desc_set(gate_desc_t * desc, uint16_t selector, uint32_t offset, uint16_t attr)
{
    desc->offset15_0  = offset & 0xffff;
    desc->selector    = selector;
    desc->attr        = attr;
    desc->offset31_16 = (offset >> 16) & 0xffff;
}

void gdt_free_sel(int sel)
{
    mutex_lock(&mutex);
    gdt_table[sel / sizeof(segment_desc_t)].attr = 0;
    mutex_unlock(&mutex);
}

/**
 * 在GDT表中分配一个空闲的段选择因子
 */
int gdt_alloc_desc(void)
{
    int i;

    mutex_lock(&mutex);
    // 跳过第0项, 该项为CPU保留
    for (i = 1; i < GDT_TABLE_SIZE; i++)
    {
        segment_desc_t * desc = gdt_table + i;
        if (desc->attr == 0)
        {
            desc->attr = SEG_P_PRESENT;     // 标记为占用状态
            break;
        }
    }
    mutex_unlock(&mutex);

    return i >= GDT_TABLE_SIZE ? -1 : i * sizeof(segment_desc_t);;
}

/**
 * 初始化GDT表
 */
static void init_gdt(void)
{
    // 先将GDT表中256个表项全部清空
    for (int i = 0; i < GDT_TABLE_SIZE; i++)
    {
        /**
         * 16位段选择因子Segment Selector
         * [15:3] Index在GDT表中索引, 因此i=Index需要左移3位
         * [2]    TI(Table Indicator), 0-GDT, 1-LDT
         * [1:0]  RPL(Request Privilege Level)请求特权级
         * i是GDT表项的索引号, i<<3 其实就是 i * sizeof(segment_desc_t) = i*8, 即i<<3
         */
        segment_desc_set(i << 3, 0, 0, 0);
    }

    /**
     * 设置代码段, 只能用非一致代码段, 以便通过调用门更改当前任务的CPL执行关键的资源访问操作
     */
    segment_desc_set(KERNEL_SELECTOR_CS, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_CODE
                     | SEG_TYPE_RW | SEG_D | SEG_G);

    // 设置数据段
    segment_desc_set(KERNEL_SELECTOR_DS, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_DATA
                     | SEG_TYPE_RW | SEG_D | SEG_G);

    // 设置系统调用门, 此接口用于提供给应用程序的API, 因此设为DPL3
    gate_desc_set((gate_desc_t *)(gdt_table + (SELECTOR_SYSCALL >> 3)),
                  KERNEL_SELECTOR_CS,
                  (uint32_t)exception_handler_syscall,
                  GATE_P_PRESENT | GATE_DPL3 | GATE_TYPE_SYSCALL | SYSCALL_PARAM_COUNT);

    // 重新加载gdt表, 使GDTR寄存器指向GDT表起始地址
    lgdt((uint32_t)gdt_table, sizeof(gdt_table));
}

/**
 * 切换至TSS，即跳转实现任务切换
 */
void switch_to_tss(uint32_t tss_selector)
{
    // 段选择因子, offset偏移量
    far_jump(tss_selector, 0); // 通过tss选择因子在GDT表中找到tss对应进程的入口起始地址entry 并运行
}

#if 0
//通过改写Eflags寄存器的第21位，观察其位的变化判断是否支持CPUID
int chk_cpuid(void)
{
    int rets = 0;
    __asm__ __volatile__(
                "pushfl \n\t"
                "popl %%eax \n\t"
                "movl %%eax,%%ebx \n\t"
                "xorl $0x0200000,%%eax \n\t"
                "pushl %%eax \n\t"
                "popfl \n\t"
                "pushfl \n\t"
                "popl %%eax \n\t"
                "xorl %%ebx,%%eax \n\t"
                "jz 1f \n\t"
                "movl $1,%0 \n\t"
                "jmp 2f \n\t"
                "1: movl $0,%0 \n\t"
                "2: \n\t"
                : "=c"(rets)
                :
                :);
    return rets;
}
//检查CPU是否支持长模式
int chk_cpu_longmode(void)
{
    int rets = 0;
    __asm__ __volatile__(
                "movl $0x80000000,%%eax \n\t"
                "cpuid \n\t"  // 把eax中放入0x80000000调用CPUID指令
                "cmpl $0x80000001,%%eax \n\t"  // 看eax中返回结果
                "setnb %%al \n\t"   // 不为0x80000001,则不支持0x80000001号功能
                "jb 1f \n\t"
                "movl $0x80000001,%%eax \n\t"
                "cpuid \n\t"  // 把eax中放入0x800000001调用CPUID指令，检查edx中的返回数据
                "bt $29,%%edx  \n\t"  // 长模式 支持位  是否为1
                "setcb %%al \n\t"
                "1: \n\t"
                "movzx %%al,%%eax \n\t"
                : "=a"(rets)
                :
                :);
    return rets;
}

// 检查CPU主函数
void init_check_cpu(void)
{
    if (!chk_cpuid())
    {
        kerror("Your CPU is not support CPUID sys is die!");
        CLI_HALT();
    }
    if (!chk_cpu_longmode())
    {
        kerror("Your CPU is not support 64bits mode sys is die!");
        CLI_HALT();
    }
    //mbsp->mb_cpumode = 0x40; // 如果成功则设置机器信息结构的cpu模式为64位
    return;
}
#endif

/**
 * CPU初始化, GDT表初始化
 */
void cpu_init(void)
{
    mutex_init(&mutex);

    // 通过获取 CPUID 指令来检查 CPU 是否支持 64 位长模式. 暂不支持64位, 略!
    //init_check_cpu();
    init_gdt();
}
