/**
 * 手写操作系统
 *
 * 文件名: init.c
 * 功能描述: 内核初始化以及测试代码
 */

#include "init.h"

#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "core/task.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"
#include "dev/console.h"
#include "dev/kbd.h"
#include "fs/fs.h"


static boot_info_t * init_boot_info;        // 启动信息


/**
 * 寄存器
 * 1) GDTR 寄存器保存GDT表的基地址
 *    GDT表中保存进程的任务段(代码段, 数据段)+TSS段64位描述符, GDT表一共256项, 前4项留给内核, 后252项给用户进程
 *    一个进程有任务段+TSS段, 因此最多可保存126个进程
 *    TR(Task Register) 寄存器保存当前正在运行任务current的任务段+TSS段的首地址
 *    每个TSS段64位描述符指向的是task_struct进程结构体中的tss_t首地址
 *
 * 2) LDTR寄存器保存LDT表的基地址
 *
 * 3) IDTR寄存器保存IDT表的基地址
 *    IDT表中保存着 Interrupt/Trap/Task Gate 这三种类型的描述符, 每个描述符占64位, IDT表一共256项,
 *    前0-31右系统固定, 后面可自定义. 每一表项对应一个中断/异常/任务门描述符, 64字节中断描述符
 */

/**
 * 1. boot_info
 * 需将boot_info_t硬件参数信息传递给内核, 有两种方式
 * 1) loader将硬件参数信息写入内存中某个固定地址, 当kernel启动后从该固定地址中取出数据并解析
 *    缺点: 需要事先约定好, 且当后续存储规划发生变化时, 需要同时调整
 * 2) 将硬件参数信息作为函数入参传入, (void (*)(boot_info_t *))kernel_entry_addr(&boot_info)
 *    优点: 不必考虑内存位置, 但需要了解一些函数调用时参数传递的知识
 */

/**
 * 内核入口函数
 * boot_info boot程序向内核传递启动信息, 包括硬件参数等
 */
void kernel_init(boot_info_t * boot_info)
{
    init_boot_info = boot_info;

    // 传入数据异常判断
    ASSERT(init_boot_info->ram_region_count != 0);

    // 初始化图形显示开机动画, TBD!!!

    // 初始化CPU, 重新加载GDT表
    cpu_init();

    // 初始化IDT表 和 PIC中断控制器
    irq_init();

    // 将log模块放在前面进行初始化, 方便后来的模块在初始化出现异常时可通过串口打印信息查看出错信息
    log_init();

    // 内存初始化要放前面一点，因为后面的代码可能需要内存分配
    memory_init(init_boot_info);

    // 文件系统初始化
    fs_init();

    time_init();

    task_manager_init();
}


/**
 * x86栈的基本结构: 栈单元大小为32位(暂时不考虑64位系统)
 * esp寄存器总是指向最后压入的栈单元, ebp寄存器总是指向当前栈的栈底
 * 且栈单元从高地址向低地址增长
 * 压栈指令:
 * 1) push
 *  esp寄存器先减4, 再将数据写入当前esp寄存器指向的地址
 * 2) pop
 *  先将esp寄存器当前指向的地址中的数据取出, 然后esp寄存器加4
 *
 * 栈的作用:
 * 保存局部变量和数据
 * 传递参数: 从参数列表右侧往左将传递给被调用函数的参数压入栈
 * 保存返回地址
 * 保存老的栈ebp0寄存器
 * 将新的栈ebp1寄存器指向当前esp寄存器
 * esp寄存器指向当前栈顶
 * 接下来就是被调用函数使用的栈空间
 * ebp寄存器始终指向栈底不变, 通过esp栈顶寄存器进行栈的压入压出操作
 * 后续通过ebp+偏移量 读取调用者传入的参数
 */

/**
 * @brief 移至第一个进程运行
 */
static void move_to_first_task(void)
{
    task_t * curr = task_current();
    ASSERT(curr != 0);

    // 获得当前任务的tss结构
    tss_t * tss = &(curr->tss);

    /**
     * 不能直接用Jmp far进入，因为当前任务的特权级为0，不能跳到低特权级的代码
     * 下面的iret后，还需要手动加载ds, fs, es等寄存器值，iret不会自动加载
     * 注意，运行下面的代码可能会产生异常: 段保护异常或页保护异常
     * 可根据产生的异常类型和错误码，并结合手册来找到问题所在
     * 也可以使用类似boot跳loader中的函数指针跳转
     */
    __asm__ __volatile__(
        // "jmp *%[ip]"::[ip]"r"(tss->eip)
        /**
         * 模拟中断返回，切换入第1个可运行应用进程
         * 不过这里并不直接进入到进程的入口，而是先设置好段寄存器，再跳过去
         */
        "push %[ss]\n\t"           // SS
        "push %[esp]\n\t"          // ESP
        "push %[eflags]\n\t"       // EFLAGS
        "push %[cs]\n\t"           // CS
        "push %[eip]\n\t"          // eip, 进程入口地址
        "iret\n\t"::[ss]"r"(tss->ss), [esp]"r"(tss->esp), [eflags]"r"(tss->eflags),
        [cs]"r"(tss->cs), [eip]"r"(tss->eip));
}

void init_main(void)
{
    log_printf("==============================");
    log_printf("Kernel is running....");
    log_printf("Version: %s, name: %s", OS_VERSION, "myos x86 arch");
    log_printf("==============================");


    // CPU的eflags寄存器第9位有个中断全局开关, 初始化完成后需将其打开
    //irq_enable_global();

    /**
     * 这里后两个入参填0, 由于first_task在kernel跑起来后已经运行, 因此不需要从tss中加载初始化的值
     * 因此这里是否填写参数都无所谓, 但在后续切换回init_task时会因保存状态而被改写
     */
    // 初始化并执行整个系统的第一个任务
    task_first_init();
    move_to_first_task();
}


/**
 * 内存中的布局:
 * 模块名       磁盘中的扇区     内存中的地址        大小
 * boot         0              0x7C00        1个扇区=512字节
 * loader       1-64           0x8000        64个扇区=32KB
 * kernel       无             0x10000        大小??
 * kernel(ELF) 100-599         0x100000      500个扇区=250KB
 */



/**
 * 分段机制
 * 逻辑地址 Logical Address 包含两部分
 * 16位 Segment Selector 段选择因子
 * 32位 offset
 *
 * [15:3] Index在GDT表中索引
 * [2]    TI(Table Indicator), 0-GDT, 1-LDT
 * [1:0]  RPL(Request Privilege Level)请求特权级
 *
 * 16位段选择因子中GDT表中查找对应表项, 步骤如下:
 * 1) 通过TI位确定是GDT还是LDT表, 这里假设是GDT
 * 2) 通过CR3寄存器找到GDT表基地址
 * 3) 通过Index>>3找到 全局gdt_table 表中对应的段描述符 Segment Descriptor
 * 4) 通过解析段描述符中base地址 + 32位offset偏移量 可求得线性地址 Linear Address
 */


/**
 * 分页机制
 * 1) 将线性地址转化成逻辑地址
 * 2) 在较小的内存上实现更大的虚拟内存
 * 3) 按需加载等功能
 */

