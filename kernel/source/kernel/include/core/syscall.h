/**
 * 手写操作系统
 *
 * 文件名: syscall.h
 * 功  能: 系统调用实现
 */

#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H


#include "comm/types.h"


#define SYSCALL_PARAM_COUNT     5      // 系统调用最大支持的参数

// 系统调用号宏定义
#define SYS_msleep              0
#define SYS_getpid              1
#define SYS_fork                2
#define SYS_execve              3
#define SYS_yield               4
#define SYS_exit                5
#define SYS_wait                6

#define SYS_open                50
#define SYS_read                51
#define SYS_write               52
#define SYS_close               53
#define SYS_lseek               54
#define SYS_isatty              55
#define SYS_sbrk                56
#define SYS_fstat               57
#define SYS_dup                 58
#define SYS_ioctl               59

#define SYS_opendir             60
#define SYS_readdir             61
#define SYS_closedir            62
#define SYS_unlink              63

#define SYS_printmsg            100


/**
 * 系统调用的栈信息
 */
typedef struct _syscall_frame_t
{
    uint32_t eflags;
    // 手动压栈的寄存器: ds, es, fs, gs
    uint32_t gs, fs, es, ds;
    // pushal指令自动压栈的寄存器: eax, ecx, edx, ebx, esp, ebp, esi, edi
    uint32_t edi, esi, ebp, dummy, ebx, edx, ecx, eax;
    // 硬件会自动压栈的寄存器: arg3, arg2, arg1, arg0, func_id, cs, eip
    uint32_t eip, cs;
    uint32_t func_id, arg0, arg1, arg2, arg3;
    uint32_t esp, ss;
} syscall_frame_t;

// syscall处理, 在 start.S 中定义, 它最终会调用 do_handler_syscall
void exception_handler_syscall(void);

// syscall处理, 在 start.S 中断处理汇编代码中进行统一处理
void exception_handler_syscall_irq(void);


#endif //OS_SYSCALL_H
