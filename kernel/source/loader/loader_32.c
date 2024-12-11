/**
 * 手写操作系统
 *
 * 文件名: loader_32.c
 * 功  能: 本程序主要目的是从磁盘读取内核image文件, 并加载到内存
 *        32位引导代码, 二级引导，负责进行硬件检测，进入保护模式，然后加载内核，并跳转至内核运行
 */

// 编译器默认按32位指令的方式生成代码, 可省略

#include "loader.h"
#include "comm/elf.h"


// 开启分页模式时的宏定义
#define PDE_P            (1 << 0)  // P - Present, 物理内存映射是否存在, 1-存在
#define PDE_PS           (1 << 7)  // PS -Page Size, 必须为1, 表项指向4MB物理内存页; 0-表项指向4KB物理内存页
#define PDE_W            (1 << 1)  // R/W读写位, 0-只读, 1-可读写
#define CR4_PSE          (1 << 4)
#define CR0_PG           (1 << 31)


/**
 * x86栈的基本结构: 栈单元大小为32位
 * 栈从高地址往低地址方向增长
 * 1) 压栈 push
 *    esp先减4, 再将数据压入栈中
 * 2) 出栈 pop
 *    先将数据弹出, esp再加4
 *
 * ebp - Extended Base Point 该指针永远指向系统栈最上面的一个栈帧的底部基地址
 *   在一个函数调用过程中, ebp通常被当作基址指针, 指向当前函数的栈帧(Stack Frame)的基址
 * esp - Extended Stack Point 该指针指向系统栈最上面的一个栈帧的顶部
 *   esp是栈指针寄存器, 它指向当前栈的栈顶的位置
 * 压栈/出栈操作都是通过改变esp寄存器来完成的, 而ebp一直不会变
 *
 * C语言函数调用过程栈中数据变化
 * 1) 保存调用者的局部变量和数据
 * 2) 传递参数, 从参数列表右侧往左压入栈中
 * 3) 保存函数返回后下一个指令运行的地址
 * 4) 通过ebp+偏移值 取调用者的传入参数
 */


/**
 * 使用LBA48位模式读取磁盘的命令序列
 * LBA - Logical Block Addressing 逻辑块寻址, 是一种用于访问硬盘驱动器上数据的方法
 * LBA48模式将硬盘上所有的扇区都看作线性排列, 没有磁盘, 柱面等概念, 序号从0开始依次访问, 访问顺序如下:
 * outb(0x1F6, 0x40 | (slavebit<<4)) 选择硬盘: 主或从盘, slavebit=0: 主; =1从
 * outb(0x1F2, sector count high 8-bits)
 * outb(0x1F3, LBA4)
 * outb(0x1F4, LBA5)
 * outb(0x1F5, LBA6)
 *
 * outb(0x1F2, sector count low 8-bits)
 * outb(0x1F3, LBA1)
 * outb(0x1F4, LBA2)
 * outb(0x1F5, LBA3)
 *
 * send the "READ SECTORS EXT" command(0x24) to port(0x1F7)
 *
 * 参考资料
 * https://wiki.osdev.org/ATA_PIO_Mode
 *
 * uint32_t sector:  起始扇区号
 * uint32_t sector_count:  读取的扇区数
 * uint8_t * buf:  读取的image加载到内存中的起始地址
 */
static void read_disk(uint32_t sector, uint32_t sector_count, uint8_t * buf)
{
    /**
     * LBA寄存器一览
     * 寄存器地址   读写     功能
     * 0x1F0      R/W      数据寄存器
     * 0x1F1      R        错误寄存器
     * 0x1F1      W        特性寄存器
     * 0x1F2      R/W      扇区数量寄存器
     * 0x1F3      R/W      sector number寄存器, low 8-bits, 扇区号寄存器
     * 0x1F4      R/W      Cylinder Low 寄存器, LBA mid
     * 0x1F5      R/W      Cylinder High 寄存器, LBA high
     * 0x1F6      R/W      Drive/Head 寄存器, 读取哪一个磁盘
     * 0x1F7      R        状态寄存器
     * 0x1F7      W        命令寄存器
     */

    /**
     * 0x1F6寄存器对应位解析如下:
     * [3:0] 在bits24-27 block number
     * [4]   DRV 驱动器号, slavebit 0: 主盘; 1从盘
     * [5]   总为1
     * [6]   LBA模式, 总是设为1
     * [7]   总为1
     * 即为1110,0000b = 0xE0
     */
    // 1. 设置读取磁盘的参数配置项和命令
    outb(0x1F6, (uint8_t) (0xE0)); // 默认主盘, 0<<0 | 0<<4 | 1<<5 | 1<<6 | 1<<7 = 0xE0

    outb(0x1F2, (uint8_t) (sector_count >> 8)); // sector count high 8-bits, 扇区数量高8位
    outb(0x1F3, (uint8_t) (sector >> 24));      // LBA参数的24~31位
    // LBA48支持48位地址, 但在这里只用到低32位, 因此高16位(LBA5,LBA6)填0
    outb(0x1F4, (uint8_t) (0));                 // LBA参数的32~39位
    outb(0x1F5, (uint8_t) (0));                 // LBA参数的40~47位

    outb(0x1F2, (uint8_t) (sector_count));      // sector count low 8-bits, 扇区数量低8位
    outb(0x1F3, (uint8_t) (sector));            // LBA参数的0~7位
    outb(0x1F4, (uint8_t) (sector >> 8));       // LBA参数的8~15位
    outb(0x1F5, (uint8_t) (sector >> 16));      // LBA参数的16~23位

    // send the "READ SECTORS EXT" command(0x24) to port(0x1F7)
    outb(0x1F7, (uint8_t) 0x24);

    /**
     * 读取数据
     * 0x1F7 Status Register 状态寄存器
     * [0]  ERR  发生错误
     * [3]  DRQ  就绪, 可以读写数据
     * [7]  BSY  忙
     */
    uint16_t * data_buf = (uint16_t *) buf; // 因为磁盘每次读写最少是16位数据, 需将类型转换一下
    while (sector_count-- > 0)
    {
        // 2. 等待操作完成
        // 每次扇区读之前都要检查，等待数据就绪并且不是忙状态, 0<<0 | 1<<3 | 0<<7 = 0x8
        while ((inb(0x1F7) & 0x88) != 0x8) {}

        // 3. 从磁盘读取2字节数据, 并将数据写入到输出缓存buf中
        for (int i = 0; i < SECTOR_SIZE / 2; i++)
        {
            *data_buf++ = inw(0x1F0); // 从数据寄存器0x1F0中读取数据, 每次读取2字节
        }
    }
}

/**
 * 解析elf文件，提取内容到相应的内存中
 *
 * 为了更好的组织程序和数据, 一般需要专用的文件格式, 如PE格式或ELF格式
 * ELF用于二进制文件、可执行文件、目标代码、共享库和核心转储格式的文件格式
 *
 * ELF格式包含以下几部分:
 * ELF header
 * program header 0
 * program header 1
 * segment0/1/2/3
 * ...
 *
 * 先解析ELF header 可定位到 program header0/1 和 segment0/1/2/3
 * 再通过找到的 program header0/1 和 segment0/1/2/3 中的信息 p_offset 和 
 * p_filesz(在ELF文件中的信息)定位到 .text代码段 和 .data数据段在文件中的位置及大小信息
 * 并通过p_addr(加载到内存中的信息, 长度还是p_filesz)作为起始地址逐个拷贝加载到内存
 * 拷贝总长度为 p_memsz = p_filesz + 清0区域
 *
 * ELF资料详细可参考下面的链接
 * ELF文件格式简介及加载过程说明:  https://wiki.osdev.org/ELF
 * ELF格式详细说明:  ../documents/ELF_Format.pdf
 */
static uint32_t reload_elf_file(uint8_t * file_buffer)
{
    // 读取的只是ELF文件，不像BIN那样可直接运行，需要从中加载出有效数据和代码
    // 简单判断是否是合法的ELF文件, 检查文件开头4字节是否为: ".ELF"
    Elf32_Ehdr * elf_hdr = (Elf32_Ehdr *)file_buffer;
    if ((elf_hdr->e_ident[0] != ELF_MAGIC) || (elf_hdr->e_ident[1] != 'E')
        || (elf_hdr->e_ident[2] != 'L') || (elf_hdr->e_ident[3] != 'F'))
    {
        return 0;
    }

    // 然后从中加载程序头，将内容拷贝到相应的位置
    for (int i = 0; i < elf_hdr->e_phnum; i++)
    {
        Elf32_Phdr * phdr = (Elf32_Phdr *)(file_buffer + elf_hdr->e_phoff) + i;
        if (phdr->p_type != PT_LOAD) // 1 - 可被加载的 Segment
            continue;

        /**
         * 全部使用物理地址，此时分页机制还未打开
         * 通过elf header中的 p_offset 和 p_filesz 索引到ELF文件中的对应 Segment
         * 再将数据拷贝到 p_addr 的内存地址处, 大小为 p_memsz
         */
        uint8_t * src = file_buffer + phdr->p_offset;
        uint8_t * dest = (uint8_t *)phdr->p_paddr;
        // 对于.text和.rodata段以下处理没问题
        for (int j = 0; j < phdr->p_filesz; j++)
        {
            *dest++ = *src++;
        }

        /**
         * 对于.text和.rodata段, 以上拷贝没有问题; 但对于.data和.bss段, 除了拷贝p_filesz长度的内存
         * 还需将后面的.bss内存区域清0(从p_filesz ~ p_memsz之间的区域), 即memsz和filesz不同时，后续要填0
         */
        dest= (uint8_t *)phdr->p_paddr + phdr->p_filesz;
        for (int j = 0; j < phdr->p_memsz - phdr->p_filesz; j++)
        {
            *dest++ = 0;
        }
    }

    // 将内核程序的入口地址返回
    return elf_hdr->e_entry;
}

/**
 * 死机
 */
static void die (int code)
{
    for (;;) { } // 死循环
}


/**
 * 开启分页机制
 * 将0-4M空间映射到0-4M和SYS_KERNEL_BASE_ADDR~+4MB空间
 * 0-4MB的映射主要用于保护loader自己还能正常工作
 * SYS_KERNEL_BASE_ADDR+4MB则用于为内核提供正确的虚拟地址空间
 */
static void enable_page_mode(void)
{
    /**
     * 此处使用4MB物理页, 这样构造页表就简单很多, 只需要1个页表即可, 这里将 0-4MB的虚拟内存 对应到 0-4MB的物理页内存
     * 以下内核内存管理的bitmap表为临时使用, 用于帮助内核正常运行
     * 内核运行起来之后, 在 kernel_init -> memory_init 中将重新设置虚拟内存与物理页表对应关系
     */
    static uint32_t page_dir[1024] __attribute__((aligned(4096))) =
    {
        //    存在的   4MB物理页 可读写  高10位为物理内存页的起始地址, 即从地址0开始, 因此线性地址与物理地址一样
        [0] = PDE_P | PDE_PS | PDE_W | 0,      // PDE_PS = 1 - Page Size = 1, 开启4MB的页
    };

    // 设置CR4寄存器, 先读取CR4寄存器的值; 再将该值或PSE, 以便启用4M的页, 而不是4KB
    uint32_t cr4 = read_cr4();
    write_cr4(cr4 | CR4_PSE);

    // 设置CR3寄存器, 将页目录基地址写到CR3寄存器中
    write_cr3((uint32_t)page_dir);

    // 开启分页机制
    uint32_t cr0 = read_cr0();
    write_cr0(cr0 | CR0_PG);
}

/**
 * 从磁盘上加载内核
 *
 * 此时内存中的布局:
 * 模块名       磁盘中的扇区     加载内存地址        大小
 * boot         0              0x7C00        1个扇区=512字节
 * loader       1-64           0x8000        64个扇区=32KB
 * kernel(ELF) 100-599         0x100000      500个扇区=250KB
 */
void load_kernel(void)
{
    /**
     * 从磁盘扇区读取kernel image文件 "临时" 放到1MB内存处
     * 读取的扇区数一定要大一些，保不准kernel.elf大小会变得很大
     * 内核在第100个扇区, 大小是500个扇区(总大小=512*500=250KB)
     *
     * 此处仅仅是将磁盘上的kernel.elf文件读取后, 临时存放到1MB内存处
     * 待下面解析ELF文件后就会将内核的代码段、数据段, 加载到0x10000=64KB内存处
     */
    read_disk(100, 500, (uint8_t *)SYS_KERNEL_LOAD_ADDR);
    // 这里的内核image没有进行压缩, 因此无需解压的操作

    /**
     * 解析ELF文件格式, 把内核image中的指令段、数据段、BSS段等, 根据ELF中信息和要求放入1MB内存处, 最后返回指令段的入口地址
     *
     * 将ELF文件从磁盘上 "临时" 先读到 SYS_KERNEL_LOAD_ADDR =1MB内存处, 再进行解析
     * 内核image文件加载到内存后需解析ELF文件内容, 从中获取内核的信息
     * 然后再将ELF的各个段拷贝到elf中指定的内存中, 起始地址为 kernel_entry(=0x10000=64KB)
     */
    uint32_t kernel_entry = reload_elf_file((uint8_t *)SYS_KERNEL_LOAD_ADDR);
    if (kernel_entry == 0) // ELF文件解析失败
    {
        die(-1); // 系统死机, -1是传入的错误码
    }

    // 开启内存分页机制
    enable_page_mode();

    /**
     * kernel_entry 即内核加载到内存中的地址(=0x10000), 该参数是从ELF文件中解析而获得
     *
     * 同时, 需将 boot_info_t * boot_info 硬件参数信息传递给内核, 有两种方式
     * 1) loader将硬件参数信息写入内存中某个固定地址, 当kernel启动后从该固定地址中取出数据并解析
     *    缺点: 需要事先约定好, 且当后续存储规划发生变化时, 需要同时调整
     * 2) 将硬件参数信息作为函数入参传入, (void (*)(boot_info_t *))kernel_entry_addr(&boot_info)
     *    优点: 不必考虑内存位置, 但需要了解一些函数调用时参数传递的知识
     */
    typedef void (*func_t)(boot_info_t *);
    func_t kernel_entry_addr = (func_t)kernel_entry;
    kernel_entry_addr(&boot_info);
    //((void (*)(boot_info_t *))kernel_entry)(&boot_info);

    for (;;) {} // 系统本不该跑到这里来!!!
}


