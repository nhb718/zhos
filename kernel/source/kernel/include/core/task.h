/**
 * 手写操作系统
 *
 * 文件名: task.h
 * 功  能: 任务管理
 */

#ifndef TASK_H
#define TASK_H


#include "comm/types.h"
#include "cpu/cpu.h"
#include "tools/list.h"
#include "fs/file.h"


#define TASK_NAME_SIZE               32            // 任务名字长度
#define TASK_TIME_SLICE_DEFAULT      10            // 时间片计数
#define TASK_OFILE_NR                128           // 最多支持打开的文件数量

#define TASK_FLAG_SYSTEM             (1 << 0)      // 系统任务


typedef struct _task_args_t
{
    uint32_t ret_addr;        // 返回地址，无用
    uint32_t argc;
    char ** argv;
} task_args_t;

/**
 * @brief 任务控制块结构
 * Task Register 进程状态段寄存器保存 当前进程的TSS段选择因子, 可在GDT表中快速找到TSS任务状态段
 * 当进程切换时需要更新这个寄存器
 */
typedef struct _task_t
{
    enum
    {
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP,
        TASK_READY,
        TASK_WAITING,
        TASK_ZOMBIE,
    } state;                      // 进程状态

    char name[TASK_NAME_SIZE];    // 任务名字

    int pid;                      // 进程的pid
    struct _task_t * parent;      // 父进程
    uint32_t heap_start;          // 进程的堆起始地址
    uint32_t heap_end;            // 进程的堆结束地址, 若两值相等则堆为空
    int status;                   // 进程执行结果

    int sleep_ticks;              // 睡眠时间

    int time_slice;               // 任务总的时间片数, 根据任务优先级计算出初始值, 在任务创建后就固定了
    int slice_ticks;              // 递减时间片计数, 每次10ms心跳来临后该值都减1, 当减到0时进行任务切换操作

    file_t * file_table[TASK_OFILE_NR];    // 任务打开的文件

    tss_t tss;                    // 当前任务状态字段, TSS段
    uint16_t tss_sel;             // 当前任务tss选择因子
    
    list_node_t run_node;         // 运行就绪队列
    list_node_t wait_node;        // 进程等待队列
    list_node_t all_node;         // 所有进程队列
} task_t;


/**
 * 任务初始化函数
 * task_t * task  进程结构体
 * const char * name  进程名
 * int flag 用户态/内核态进程, 1-内核态, 0-用户态
 * uint32_t entry  进程入口起始地址, 即进程执行的入口函数指针
 * uint32_t esp  进程栈顶指针
 */
int task_init(task_t * task, const char * name, int flag, uint32_t entry, uint32_t esp);

/**
 * 任务切换函数
 * 参考文档: ../documents/325384-sdm-vol-3abcd.pdf  Chapter 7.3 Task Switching
 * 由于使用硬件来完成TSS切换, 在此过程中会对权限做检查, 还有整个TSS任务状态段都会切换, 因此该方法效率较低
 * 其实只需保存必要的寄存器即可
 * 1) 通用寄存器: eax, ebx, ecx, edx, ebp, esp, esi, edi
 * 2) 运行状态寄存器: eflags, eip
 * 段寄存器在所有任务中都一样, 不用保存
 * 将上述寄存器保存到当前任务的栈中, 其中
 * 无需保存: eax, ecx, edx, eflags, eip(已经保存在当前任务栈中)
 * 需要保存: ebx, ebp, esp, esi, edi
 *
 * 参考文档:  ../documents/abi386-4.pdf  Page 37
 */
void task_switch_from_to(task_t * from, task_t * to);
void task_set_ready(task_t * task);
void task_set_block(task_t * task);
void task_set_sleep(task_t *task, uint32_t ticks);
void task_set_wakeup(task_t *task);
int sys_yield(void);
void task_dispatch(void);
/**
 * 在进程运行过程中, 很多原因需要等待, 从而加入相应的等待队列
 * 1) 就绪队列
 * 2) 等待队列
 */
task_t * task_current(void);
void task_time_tick(void);
void sys_msleep(uint32_t ms);
file_t * task_file(int fd);
int task_alloc_fd(file_t * file);
void task_remove_fd(int fd);


typedef struct _task_manager_t
{
    task_t * curr_task;           // 当前运行的任务

    list_t ready_list;            // 就绪队列
    list_t task_list;             // 所有已创建任务的队列
    list_t sleep_list;            // 延时队列, 睡眠等待队列

    task_t first_task;            // 第一个应用任务
    task_t idle_task;             // 空闲任务

    int app_code_sel;             // 应用任务代码段选择子
    int app_data_sel;             // 应用任务的数据段选择子
} task_manager_t;

void task_manager_init(void);
void task_first_init(void);
task_t * task_first_task(void);

int sys_getpid(void);
int sys_fork(void);
int sys_execve(char *name, char **argv, char **env);
void sys_exit(int status);
int sys_wait(int* status);


#endif

