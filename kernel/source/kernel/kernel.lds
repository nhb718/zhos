/**
 * lds链接脚本文件可控制整个程序中 代码段和数据段的存放位置
 *
 * 参考文档
 * GCC LD官方文档:  https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_chapter/ld_3.html
 * linker脚本:  https://wiki.osdev.org/Linker_Scripts
 * ARM Linker链接脚本:  https://www.bravegnu.org/gnu-eprog/linker.html
 * ld脚本编写:  https://sourceware.org/binutils/docs/ld
 */

SECTIONS
{
    PROVIDE(kernel_base = 0x0);

    /* 指定程序的起始地址 0x10000 = 64KB */
    . = 0x0010000;

    . = ALIGN(4096);
    PROVIDE(s_text = .);
    /* 代码段, 将first_task相关代码/数据都去掉 */
    .text :
    {
        *(EXCLUDE_FILE(*first_task* *lib_syscall*) .text)
    }

    /* 只读数据段, 将first_task相关代码/数据都去掉 */
    .rodata :
    {
        *(EXCLUDE_FILE(*first_task* *lib_syscall*) .rodata)
    }
    PROVIDE(e_text = .);

    . = ALIGN(4096);
    PROVIDE(s_data = .);
    /* 全局数据段, 将first_task相关代码/数据都去掉 */
    .data :
    {
        *(EXCLUDE_FILE(*first_task* *lib_syscall*) .data)
    }

    /* 全局未初始化数据段, 将first_task相关代码/数据都去掉 */
    .bss :
    {
        *(EXCLUDE_FILE(*first_task* *lib_syscall*) .bss)
    }
    e_data = .;

    /* first_task 初始进程的配置: 紧接着在低端1MB内, 内核数据段后的e_data开始存储, 但是运行时搬运到虚拟地址 0x80000000 处 */
    . = 0x80000000;
    PROVIDE(s_first_task = LOADADDR(.first_task));
    .first_task : AT(e_data)
    {
        *first_task_entry*(.text .data. rodata .data)
        *first_task*(.text .data. rodata .data)
        *lib_syscall*(.text .data .bss)
    }
    PROVIDE(e_first_task = LOADADDR(.first_task) + SIZEOF(.first_task));

    /* mem_free_start 开始的这块内存区域 存放内核内存管理的bitmap位图映射表 */
    PROVIDE(mem_free_start = e_first_task);
}
