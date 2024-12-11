/**
 * 手写操作系统
 *
 * 文件名: lib_syscall.c
 * 功  能: 系统调用接口
 */

#include "core/syscall.h"
#include "os_cfg.h"
#include "lib_syscall.h"
#include "malloc.h"
#include <string.h>


/**
 * 1. 多态原理
 *   静态    重载 编译时确定    参数类型、个数、const
 *               早绑定  编译时已确定对象调用的函数地址
 *               通过函数名的生成规则: ZN + 类长度 + 类名 + 函数长度 + 函数名 + 类型首字母
 *   动态    虚函数 运行时确定  基类 virtual + 函数声明 派生类重写override该函数
 *               运行时根据对象类型来调用相应的函数, 有virtual函数时为类生成虚表, 用于存放该类的需函数地址列表
 *               类对象构造时对象首元素为虚表地址, 指向该虚表
 *   
 *           
 */

/**
 * 执行系统调用
 */
static inline int sys_call(syscall_args_t * args)
{
    int ret;

    #ifdef SYSCALL_MODE
    // 1. 执行 "call + 系统调用门选择因子segment selector + 偏移量offset=0" 的指令
    const unsigned long sys_gate_addr[] = {0, SELECTOR_SYSCALL | 0};  // 偏移量offset=0; 使用特权级GATE_RPL0

    /**
     * 采用调用门, 这里只支持5个参数
     * 用调用门的好处是会自动将参数复制到内核栈中，这样内核代码很好取参数
     * 而如果采用寄存器传递，取参比较困难，需要先压栈再取
     */
    __asm__ __volatile__(
            "push %[arg3]\n\t"      // 将5个参数都压入栈中
            "push %[arg2]\n\t"
            "push %[arg1]\n\t"
            "push %[arg0]\n\t"
            "push %[id]\n\t"
            "lcalll *(%[gate])\n\n" // 执行lcall 指令
            :"=a"(ret)  // 出参, 将返回值的eax寄存器中的值赋值给ret并返回
            // 入参, 5个参数都压入栈中
            :[arg3]"r"(args->arg3), [arg2]"r"(args->arg2), [arg1]"r"(args->arg1),
            [arg0]"r"(args->arg0), [id]"r"(args->id),
            [gate]"r"(sys_gate_addr));

    #else
    // 也可通过中断门 int $0x80 指令实现系统调用, 通过5个通用寄存器来传递参数, 无需压栈
    __asm__ __volatile__(
            "int $0x80"
            :"=a"(ret) // 出参
            // 入参, 5个参数分别写入通用寄存器中
            : "S"(args->arg3), "d"(args->arg2), "c"(args->arg1),
            "b"(args->arg0), "a"(args->id)
    );
    #endif

    return ret;
}

int msleep(int ms)
{
    if (ms <= 0)
    {
        return 0;
    }

    syscall_args_t args;
    args.id = SYS_msleep;
    args.arg0 = ms;

    return sys_call(&args);
}

int getpid(void)
{
    syscall_args_t args;
    args.id = SYS_getpid;

    return sys_call(&args);
}

int print_msg(char * fmt, int arg)
{
    syscall_args_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;

    return sys_call(&args);
}

int fork(void)
{
    syscall_args_t args;
    args.id = SYS_fork;

    return sys_call(&args);
}

int execve(const char * name, char * const * argv, char * const * env)
{
    syscall_args_t args;
    args.id = SYS_execve;
    args.arg0 = (int)name;
    args.arg1 = (int)argv;
    args.arg2 = (int)env;

    return sys_call(&args);
}

int yield(void)
{
    syscall_args_t args;
    args.id = SYS_yield;

    return sys_call(&args);
}

int wait(int * status)
{
    syscall_args_t args;
    args.id = SYS_wait;
    args.arg0 = (int)status;

    return sys_call(&args);
}

void _exit(int status)
{
    syscall_args_t args;
    args.id = SYS_exit;
    args.arg0 = (int)status;
    sys_call(&args);
    for (;;) {}
}

int open(const char * name, int flags, ...)
{
    // 不考虑支持太多参数
    syscall_args_t args;
    args.id = SYS_open;
    args.arg0 = (int)name;
    args.arg1 = (int)flags;

    return sys_call(&args);
}

int read(int file, char * ptr, int len)
{
    syscall_args_t args;
    args.id = SYS_read;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = len;

    return sys_call(&args);
}

int write(int file, char * ptr, int len)
{
    syscall_args_t args;
    args.id = SYS_write;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = len;

    return sys_call(&args);
}

int close(int file)
{
    syscall_args_t args;
    args.id = SYS_close;
    args.arg0 = (int)file;
    return sys_call(&args);
}

int lseek(int file, int ptr, int dir)
{
    syscall_args_t args;
    args.id = SYS_lseek;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = dir;

    return sys_call(&args);
}

/**
 * 获取文件的状态
 */
int fstat(int file, struct stat * st)
{
    syscall_args_t args;
    args.id = SYS_fstat;
    args.arg0 = (int)file;
    args.arg1 = (int)st;

    return sys_call(&args);
}

/**
 * 判断文件描述符与tty关联
 */
int isatty(int file)
{
    syscall_args_t args;
    args.id = SYS_isatty;
    args.arg0 = (int)file;

    return sys_call(&args);
}

void * sbrk(ptrdiff_t incr)
{
    syscall_args_t args;
    args.id = SYS_sbrk;
    args.arg0 = (int)incr;

    return (void *)sys_call(&args);
}

int dup(int file)
{
    syscall_args_t args;
    args.id = SYS_dup;
    args.arg0 = file;

    return sys_call(&args);
}

int ioctl(int fd, int cmd, int arg0, int arg1)
{
    syscall_args_t args;
    args.id = SYS_ioctl;
    args.arg0 = fd;
    args.arg1 = cmd;
    args.arg2 = arg0;
    args.arg3 = arg1;

    return sys_call(&args);
}

DIR * opendir(const char * name)
{
    DIR * dir = (DIR *)malloc(sizeof(DIR));
    if (dir == (DIR *)0)
    {
        return (DIR *)0;
    }

    syscall_args_t args;
    args.id = SYS_opendir;
    args.arg0 = (int)name;
    args.arg1 = (int)dir;
    int err = sys_call(&args);
    if (err < 0)
    {
        free(dir);
        return (DIR *)0;
    }

    return dir;
}

dirent_t * readdir(DIR * dir)
{

    syscall_args_t args;
    args.id = SYS_readdir;
    args.arg0 = (int)dir;
    args.arg1 = (int)&dir->dirent;
    int err = sys_call(&args);
    if (err < 0)
    {
        return (dirent_t *)0;
    }

    return &dir->dirent;
}

int closedir(DIR * dir)
{
    syscall_args_t args;
    args.id = SYS_closedir;
    args.arg0 = (int)dir;
    sys_call(&args);

    free(dir);

    return 0;
}

int unlink(const char * path)
{
    syscall_args_t args;
    args.id = SYS_unlink;
    args.arg0 = (int)path;

    return sys_call(&args);
}
