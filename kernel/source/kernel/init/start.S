/**
 * 手写操作系统
 *
 * 文件名: start.S
 * 功能描述: 内核启动程序, 负责解析引导程序传递的 boot_info 参数, 系统的初始化(包括硬件组件和各内核模块)
 */

#include "os_cfg.h"


// 不必加.code32, 保护模式下默认就是32位
.text
.global _start_kernel
.extern kernel_init
.extern init_main

# void start(boot_info_t * boot_info)
# 此处需将boot_info参数传递给call kernel_init, 即需要先将boot_info参数压栈, 再调用kernel_init
_start_kernel:
    // 汇编指令语法格式: mov source, dest
    # 第一种方法
    # push %ebp           // 保存ebp, 入栈
    # mov %esp, %ebp      // 将esp赋值给ebp
    # mov 0x8(%ebp), %eax // 取出boot_info参数(ebp后面有返回值+参数, 因此ebp需要+8), 并保存到eax寄存器中
    # push %eax           // 保存eax, 入栈

    # 第二种方法
    # mov 4(%esp), %eax   // 取出boot_info参数(esp后面有返回值, 因此esp需要+4), 并保存到eax寄存器中
    # push %eax           // 保存eax, 入栈

    # 第三种方法
    push 4(%esp) // 直接将esp+4指向的boot_info参数入栈, 因为拉起内核后不会再返回了, 无需保存ebp栈底指针

    # kernel_init(boot_info), 作为内核初始化函数入口, 该函数在 init.c 中实现
    // 调用void kernel(boot_info_t * boot_info)运行内核, 同时传入boot_info硬件参数信息
    call kernel_init  // call指令调用完成后, 还会返回

    // 重新加载GDT, 指定的段选择因子会被加载到CS寄存器中
    jmp $KERNEL_SELECTOR_CS, $gdt_reload

gdt_reload:
    mov $KERNEL_SELECTOR_DS, %ax   // 16位数据段选择子
    mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // 设置内核栈, 分配一个8KB的栈供内核使用
    mov $(stack + KERNEL_STACK_SIZE), %esp

    // 栈和段等沿用之前的设置
    jmp init_main  // jmp指令跳转后, 不会再返回

    .bss
.comm stack, KERNEL_STACK_SIZE    // .comm 声明未初始化的通用的8KB栈内存区域, 以字节计



// 中断发生时, 会自动切换到特权级0对应的栈中去执行
// 并且只保存ss,esp,cs,eip,flags寄存器
// 所以需要在中断中自行保存其它寄存器

// 下面的代码基本上是对上面代码的重复, 只不过换用了宏来处理
// 注意确定没写错, 可对照反汇编来看. 另外, 下面的代码没有办法调试, 原因未知
.text
.macro exception_handler name num with_error_code
.extern do_handler_\name
.global exception_handler_\name
exception_handler_\name:
    // 如果没有错误码, 压入一个缺省值
    // 这样堆栈就和有错误码的情形一样了
    .if \with_error_code == 0
        push $0  // 压入0表示没有错误码
    .endif

    // 压入异常向量号, 即中断号
    push $\num

    // 一、保护 CPU 寄存器，即中断发生时的程序运行的上下文
    # 需保存中断上下文, 有以下三种类型
    # 1. 硬件会自动压栈的寄存器: eflags, cs, eip, error code错误码(可选)
    # 2. X86 pushal指令会自动压栈通用寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    pushal
    # 3. 手动压栈的寄存器: ds, es, fs, gs
    push %ds
    push %es
    push %fs
    push %gs
    # 4. 返回地址, call指令自动压入

    // 二、调用中断处理函数, 这个程序可以是修复异常的, 也可以是设备驱动程序中对设备响应的程序
    // 在调用函数前将当前esp栈顶指针压栈, 方便在do_default_handler打印栈中整个上下文的所有寄存器信息
    push %esp
    call do_handler_\name
    add $(1*4), %esp    // 丢掉esp

    // 三、恢复 CPU 寄存器, 即恢复中断时程序运行的上下文, 使程序能继续运行
    # 恢复中断上下文, 保存时逆序即可
    # 1. 手动出栈的寄存器
    pop %gs
    pop %fs
    pop %es
    pop %ds
    # 2. popal指令出栈通用寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    popal
    # 3. 硬件自动出栈的寄存器
    # 4. 返回地址, call指令自动压入
    # pop $\num
    # pop err_code
    // 跳过压入的异常号和错误码
    add $(2*4), %esp

    iret
.endm


# 使用上述宏定义来初始化中断向量表中的异常处理函数
exception_handler unknown, -1, 0    # 未知异常
exception_handler divider, 0, 0     # 除0异常
exception_handler Debug, 1, 0       # 调试
exception_handler NMI, 2, 0         # 不可屏蔽中断
exception_handler breakpoint, 3, 0  # 断点
exception_handler overflow, 4, 0    # 栈溢出
exception_handler bound_range, 5, 0 # 边界异常
exception_handler invalid_opcode, 6, 0  # 非法指令
exception_handler device_unavailable, 7, 0 # 设备不可用
exception_handler double_fault, 8, 1
exception_handler invalid_tss, 10, 1   # TSS无效
exception_handler segment_not_present, 11, 1  # 段不存在
exception_handler stack_segment_fault, 12, 1  # 栈段错误
exception_handler general_protection, 13, 1   # 非法权限访问
exception_handler page_fault, 14, 1           # 页访问异常
exception_handler fpu_error, 16, 0
exception_handler alignment_check, 17, 1
exception_handler machine_check, 18, 0
exception_handler smd_exception, 19, 0
exception_handler virtual_exception, 20, 0

// 硬件中断
exception_handler timer, 0x20, 0      # 超时定时器
exception_handler kbd, 0x21, 0
exception_handler ide_primary, 0x2E, 0

exception_handler syscall_irq, 0x80, 0 # 系统调用中断


// eax, ecx, edx, eflags 切换时无需保存
// ebx, esi, edi, ebp 需要由被调用者保存和恢复 !!!
// cs/ds/es/fs/gs/ss 切换时无需保存
// eip 即返回地址, 已保存在进程栈中, 切换时无需保存
// esp 切换时无需保存, 只需要让esp保存在调用之前的状态
# simple_switch(uint32_t ** from, uint32_t * to)
.text
.global simple_switch
simple_switch:
    movl 4(%esp), %eax   // 取传入参数from->stack, 保存from->stack到 eax 寄存器
    movl 8(%esp), %edx   // 取传入to->stack, 保存to->stack到 edx 寄存器

    // 保存前一任务的状态寄存器
    push %ebp
    push %ebx
    push %esi
    push %edi

    // 切换栈, 将之前保存在edx寄存器中 to->stack 传给 esp
    mov %esp, (%eax)     // from->stack = esp
    mov %edx, %esp       // esp = to->stack

    // 加载下一任务的状态寄存器
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

.global exception_handler_syscall
.extern do_handler_syscall
exception_handler_syscall:
    # 需保存系统调用上下文, 有以下三种类型
    # 1. 硬件会自动压栈的寄存器: ss, esp, arg0～4, cs, eip
    # 2. X86 pushal指令会自动压栈通用寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    // 保存前一任务的状态
    pusha
    # 3. 手动压栈的寄存器: ds, es, fs, gs
    push %ds
    push %es
    push %fs
    push %gs
    pushf

    // 使用内核段寄存器, 避免使用应用层的. 此处是将eax保存的系统调用返回值给到应用层
    mov $(KERNEL_SELECTOR_DS), %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs

    // 调用处理函数
    mov %esp, %eax
    push %eax
    # 4. 返回地址, call指令自动压入
    call do_handler_syscall
    add $4, %esp

    # 恢复中断上下文, 保存时逆序即可
    # 1. 手动出栈的寄存器
    // 再切换回来
    popf
    pop %gs
    pop %fs
    pop %es
    pop %ds
    # 2. popal指令出栈通用寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    popa

    // 硬件会自动压栈的寄存器: ss, esp, arg0~4, cs, eip
    // 系统调用传递的5个参数也被硬件自动压入栈中, 此时栈中顺序为: ... ss, esp, arg0~4, cs, eip ...
    // 因此出栈时也需要跳过5个参数, 总计5*4=20字节, 不加会导致返回时ss, esp等寄存器不能正确取出, 返回到用户态时出现问题
    retf $(5*4)    // CS发生了改变, 需要使用远跳转才能从内核态返回用户态, far return 即返回到不同的代码段中

