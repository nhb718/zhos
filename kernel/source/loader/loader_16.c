/**
 * 手写操作系统
 *
 * 文件名: loader_16.c
 * 功  能: 16位引导代码, 二级引导，负责进行硬件检测，从16位实模式切换到32位保护模式，然后加载内核，并跳转至内核运行
 */

// 16位代码, 必须加上放在开头, 以下代码都会编译成16位指令, 只能在实模式下执行. 以便有些io指令生成为32位
__asm__(".code16gcc");

#include "loader.h"


boot_info_t boot_info; // 启动参数信息, boot在加载loader程序时传入


// GDT表, 只在16位实模式下使用，后面切换到32位保护模式后会替换成自己的
static uint16_t gdt_table[][4] =
{
    {0, 0, 0, 0},
    {0xFFFF, 0x0000, 0x9A00, 0x00CF},
    {0xFFFF, 0x0000, 0x9200, 0x00CF},
};


/**
 * 内联汇编, 即在C语言中嵌入汇编程序
 * 内联汇编基本格式
 * asm(汇编语句
 * : 输出操作数 (可选)
 * : 输入操作数 (可选)
 * : 被破坏的寄存器列表 (可选)
 * );
 *
 * int a=10, b;
 * asm("movl %1, %%eax; movl %%eax, %0;"
 * : "=r"(b) // 输出
 * : "r"(a)  // 输入
 * : "%eax"  // 标注破坏的寄存器
 * );
 * 其效果等于"movl a, %%eax; movl %%eax, b", 实现b = a
 * 
 * asm("mov $3, %[out]":[out]"=r"(c));
 *
 * 参考资料:
 * https://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
 * https://wiki.osdev.org/Inline_Assembly
 * https://wiki.osdev.org/Inline_Assembly/Examples
 */

/**
 * BIOS下显示字符串
 */
static void show_msg(const char * msg)
{
    char c;

    // 使用bios的 int $0x10 中断来实现往显存写数据，持续往下写
    while ((c = *msg++) != '\0')
    {
        // "r"代表通用寄存器
        __asm__ __volatile__(
            "mov $0xe, %%ah\n\t"
            "mov %[ch], %%al\n\t"
            "int $0x10"
            ::[ch]"r"(c)
            );
    }
}

/**
 * 内存检测方法: int 0x15, EAX=0xE820
 * 第一次调用时, ES:DI存储保存读取的信息的存储位置
 * 清除EBX, 设置为0
 * EDX需设置成: 0x534D4150
 * EAX设置成: 0xE820
 * ECX设置成: 24
 * 执行INT $0x15
 * 返回结果: EAX = 0x534D4150, CF标志清0, EBX被设置成某个数值用于下次调用, CL=实际读取的字节数
 * 然后从ES:DI中读取内存信息, CL=实际读取的字节数
 *
 * 后续调用:
 * EDX需重新设置成: 0x534D4150
 * EAX重新设置成: 0xE820
 * ECX重新设置成: 24
 * 执行INT $0x15
 * 返回结果: EAX = 0x534D4150, CF标志清0, 如果EBX=0则表明读取完毕; 否则当前条目有效
 *
 * 参考资料, 1MB以下比较标准, 在1M以上会有差别
 * https://wiki.osdev.org/Memory_Map_(x86)
 * https://wiki.osdev.org/Detecting_Memory_(x86)
 * https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_AH_.3D_0xC7
 */
static void detect_memory(void)
{
    uint32_t contID = 0;
    SMAP_entry_t smap_entry;
    int signature, bytes;

    show_msg("try to detect memory:");

    // 初次：EDX=0x534D4150,EAX=0xE820,ECX=24,INT 0x15, EBX=0 (初次)
    // 后续：EAX=0xE820,ECX=24,
    // 结束判断：EBX=0
    boot_info.ram_region_count = 0;
    for (int i = 0; i < BOOT_RAM_REGION_MAX; i++)
    {
        SMAP_entry_t * entry = &smap_entry;

        __asm__ __volatile__("int  $0x15"
            : "=a"(signature), "=c"(bytes), "=b"(contID)
            : "a"(0xE820), "b"(contID), "c"(24), "d"(0x534D4150), "D"(entry));
        if (signature != 0x534D4150) // EAX = 0x534D4150, CF标志清0
        {
            show_msg("failed.\r\n");
            return;
        }

        // 判断长度是否超过20字节且ACPI为0, 则忽略
        if (bytes > 20 && (entry->ACPI & 0x0001) == 0)
        {
            continue;
        }

        // 保存RAM信息，只取32位，空间有限无需考虑更大容量的情况
        if (entry->Type == 1) // 可用的RAM空间
        {
            boot_info.ram_region_cfg[boot_info.ram_region_count].start = entry->BaseL;
            boot_info.ram_region_cfg[boot_info.ram_region_count].size  = entry->LengthL;
            boot_info.ram_region_count++;
        }

        /**
         * 最后获得两段可用的RAM内存
         * 0 ~ 640KB
         * 1MB ~ 128MB
         */
        if (contID == 0) // 读取结束, 如果EBX=0则表明读取完毕
            break;
    }

    show_msg("detect memory ok.\r\n");
}



/**
 * 从16位实模式切换到32位保护模式
 *     Real Address mode
 *
 * Reset or PE=0     PE=1
 *
 *     Protected mode
 *
 *  LME=1        CR0,PG=1
 *
 * IA-32e mode(64位模式)
 *
 * 实模式
 * 1) 只能访问1MB内存(16+4位), 内核寄存器位宽最大16bits
 * 2) 所有操作数最大位宽为16bits
 * 3) 没有任何保护机制
 * 4) 没有特权级支持
 * 5) 没有分页机制和虚拟内存的支持
 *
 * 保护模式
 * 1) 寄存器位宽扩展至32位, 最大可访问4GB内存
 * 2) 所有操作数最大位宽为32bits, 出入栈也为32位
 * 3) 提供4级特权模式, 操作系统可运行在最高特权级, 应用程序可运行在最低特权级
 * 4) 支持虚拟内存, 可开启分页机制
 */

/**
 * 实模式切换到保护模式, 需要遵循以下流程
 * 1) 禁用中断, 防止中途发生中断, 程序运行出现异常
 * 2) 打开A20地址线, 为保证兼容, 默认A20地址线不开启, 无法访问4GB
 * 3) 加载GDT表, 重要的配置数据
 * 4) 开启保护模式的使能位PE=1, 设置CR0
 * 5) 远跳转, 清空原来的流水线, 取消原16位的指令
 */

/**
 * 进入保护模式
 */
static void enter_protect_mode(void)
{
    // 1. 关中断, 防止在模式切换过程中被打断
    cli();

    /**
     * 2. 开启A20地址线，使得可访问1M以上空间, 使用的是Fast A20 Gate方式
     * in al, 0x92
     * or al, 0x2
     * out 0x92, al
     *
     * 参考资料
     * 开启A20地址线方法: https://wiki.osdev.org/A20#Fast_A20_Gate
     * 实模式: https://wiki.osdev.org/Real_Mode
     * A20地址线: https://blog.csdn.net/sinolover/article/details/93877845
     */
    uint8_t v = inb(0x92);
    outb(0x92, v | 0x2);

    // 3. 加载GDT, 由于中断已经关掉, IDT不需要加载
    lgdt((uint32_t)gdt_table, sizeof(gdt_table));

    /**
     * 4. 打开CR0的保护模式位，进入保护模式
     * CR0寄存器的[0]是PE, 将该值设为1即可
     */
    uint32_t cr0 = read_cr0();
    write_cr0(cr0 | (1 << 0));

    /**
     * 5. 长跳转进入到保护模式
     * 使用长跳转，以便清空流水线，将里面的16位代码给清空
     * CPU使用的是流水线指令执行, 因此需要将之前的16位指令清空, 保证切换完后执行的是32位指令
     * protect_mode_entry是保护模式入口函数, 在./start.S 中定义
     */
    far_jump(8, (uint32_t)protect_mode_entry);
}



void loader_entry(void)
{
    // 1. 显示字符
    show_msg("....loading.....\r\n");
    // 2. 内存检测
    detect_memory();
    // 3. 从16位实模式切换到32位保护模式
    enter_protect_mode();

    for(;;) {}
}



