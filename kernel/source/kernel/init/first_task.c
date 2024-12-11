/**
 * 手写操作系统
 *
 * 文件名: first_task.c
 * 功能描述: 内核初始化以及测试代码
 */

#include "applib/lib_syscall.h"
#include "dev/tty.h"


/**
 * linux使用busybox作为应用程序
 * 这里自己写了个应用程序 first_task
 */
int first_task_main(void)
{
#if 0
    int count = 3;

    int pid = getpid();
    print_msg("first task id=%d", pid);

    pid = fork();
    if (pid < 0) {
        print_msg("create child proc failed.", 0);
    } else if (pid == 0) {
        print_msg("child: %d", count);

        char * argv[] = {"arg0", "arg1", "arg2", "arg3"};
        execve("/shell.elf", argv, (char **)0);
    } else {
        print_msg("child task id=%d", pid);
        print_msg("parent: %d", count);
    }

    pid = getpid();
    for (;;) {
        print_msg("task id = %d", pid);
        msleep(1000);
    }
#endif

    for (int i = 0; i < TTY_NR; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            print_msg("create shell proc failed", 0);
            break;
        }
        else if (pid == 0)
        {
            // 子进程
            char tty_num[] = "/dev/tty?";
            tty_num[sizeof(tty_num) - 2] = i + '0';
            char * argv[] = {tty_num, (char *)0};
            execve("shell.elf", argv, (char **)0);
            print_msg("create shell proc failed", 0);
            while (1)
            {
                msleep(10000);
            }
        }
    }

    while (1)
    {
        // 不断收集孤儿进程
        int status;
        wait(&status);
        //msleep(10000);
    }

    return 0;
}