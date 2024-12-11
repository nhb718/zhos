/**
 * 手写操作系统
 *
 * 文件名: lib_syscall.h
 * 功  能: 系统调用接口
 */

#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H


#include "core/syscall.h"
#include "os_cfg.h"
#include "fs/file.h"
#include "dev/tty.h"

#include <sys/stat.h>


/**
 * 系统调用工作流程
 * 1. 执行 "call + 系统调用门选择因子segment selector + 偏移量offset=0" 的指令
 * 2. 硬件从该指令中找到门选择因子segment selector, 再根据该选择因子从GTD表中找到
 *    系统调用门描述符Call-Gate Descriptor, 从而获取对应的代码段选择因子segment selector和代码段偏移offset
 * 3. 再根据代码段选择因子在GDT表中找到代码段描述符Code-Segment Descriptor, 从而获取对应的起始地址base
 * 4. 有了代码段起始地址base, 结合上面的代码段偏移offset, 就可找到系统调用处理函数的入口地址procedure entry point
 */

// 系统调用参数最多5个, 保护系统调用号id, arg0~4
typedef struct _syscall_args_t
{
    int id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
} syscall_args_t;

typedef struct _dirent_t
{
   int index;           // 在目录中的偏移
   int type;            // 文件或目录的类型
   char name[255];     // 目录或目录的名称
   int size;            // 文件大小
} dirent_t;

typedef struct _DIR
{
    int index;          // 当前遍历的索引
    dirent_t dirent;
} DIR;


int msleep(int ms);
int fork(void);
int getpid(void);
int yield(void);
int execve(const char * name, char * const * argv, char * const * env);
int print_msg(char * fmt, int arg);
int wait(int * status);
void _exit(int status);

int open(const char * name, int flags, ...);
int read(int file, char * ptr, int len);
int write(int file, char * ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);
int isatty(int file);
int fstat(int file, struct stat *st);
void * sbrk(ptrdiff_t incr);
int dup(int file);
int ioctl(int fd, int cmd, int arg0, int arg1);

DIR * opendir(const char * name);
dirent_t * readdir(DIR * dir);
int closedir(DIR * dir);
int unlink(const char * pathname);


#endif //LIB_SYSCALL_H
