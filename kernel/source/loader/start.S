/**
 * 手写操作系统
 *
 * 文件名: start.S
 * 功能描述: 二级引导程序, 负责进行硬件检测, 进入保护模式, 然后加载内核, 并跳转至内核运行
 */

// 16位实模式代码务必加上
.code16
.text
.global _start // 声明为全局符号, 其他文件可引用该符号
.extern loader_entry // 引用的外部符号, 在其他文件中有定义

// 以下程序为二级引导loader程序, 它会被一级引导程序boot从磁盘第2号扇区加载到内存0x8000处并执行
_start:
    // 栈和段等沿用之前的设置(cs,ds,fs,gs等), 也可以重新设置
    // 这里简单起见，就不做任何设置了, 只设置了栈顶寄存器的值0x8000
    mov $_start, %esp // mov $0x8000, %esp

    // 你可能会想, 在一级引导程序 boot中直接跳到 loader_entry, 但这样需要先知道 loader_entry 在哪儿
    // 然后boot 暂时没有这个能力, 因此loader开头处用汇编实现, 以便 _start 处于整个bin文件开头, 这样boot直接跳到开头就可以
    // loader_entry 在 loader_16.c 中定义, 负责完成从16位实模式切换到32位保护模式
    jmp loader_entry


.code32 // 告诉编译器下面的代码都是运行在32位保护模式下的代码
.text   // 告诉编译器下面的都编译到代码段中
.global protect_mode_entry // 将 protect_mode_entry 符号声明为全局, 供其他地方/文件使用
.extern load_kernel // 引用全局符号 load_kernel, 在loader_32.c中定义
protect_mode_entry:
    // 此时代码段是32位寄存器, 但其他寄存器还是16位, 需重新加载所有的数据段描述符, 使之从16位寄存器切换到32位寄存器
    mov $16, %ax  // 16位数据段选择子
    mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // 长跳转进入到32位内核加载模式中
    jmp $8, $load_kernel


