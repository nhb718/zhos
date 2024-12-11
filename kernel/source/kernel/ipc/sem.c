/**
 * 手写操作系统
 *
 * 文件名: sem.c
 * 功  能: 计数信号量
 */

#include "cpu/irq.h"
#include "core/task.h"
#include "ipc/sem.h"
#include "ipc/spinlock.h"


/**
 * 信号量
 * 适用场景: 适合长时间等待的情况, 因为有很多资源（数据）它有一定的时间性
 * 当想去获取它时, CPU 并不能立即返回, 而是要等待一段时间, 才能将数据返回
 * 这种情况时若用自旋锁来同步访问这种临界资源, 这是对 CPU 时间的巨大浪费
 *
 * 需要解决三个问题: 等待、互斥、唤醒 (即重新激活等待的代码执行流)
 * 1) 根据上面的问题, 这个数据结构至少需要一个变量来表示互斥 sem_count
 *    如大于 0 则代码执行流可以继续运行, 等于 0 则让代码执行流进入等待状态
 * 2) 还需要一个等待链，用于保存等待的代码执行流 sem_wait_list
 *
 * 具体步骤如下:
 * 1. 获取信号量
 *   1.1) 首先对用于保护信号量自身的自旋锁 sem_lock 进行加锁
 *   1.2) 对信号值 sem_count 执行“减 1”操作, 并检查其值是否小于 0
 *   1.3) 上步中检查 sem_count 如果小于 0, 就让进程进入等待状态并且将其挂入 sem_wait_list 中, 然后调度其它进程运行
 *   1.4) 否则表示获取信号量成功
 *   1.5) 最后别忘了对自旋锁 sem_lock 进行解锁
 * 2. 代码执行流开始执行相关操作, 访问共享临界资源, 如: 读取键盘缓冲区
 * 3. 释放信号量
 *   3.1) 首先对用于保护信号量自身的自旋锁 sem_lock 进行加锁
 *   3.2) 对信号值 sem_count 执行“加 1”操作，并检查其值是否大于 0
 *   3.3) 上步中检查 sem_count 值如果大于 0, 就执行唤醒 sem_waitlst 中进程的操作, 并且需要调度进程时就执行进程调度操作
 *   3.4) 不管 sem_count 是否大于 0（通常会大于 0）都标记信号量释放成功
 *   3.5) 当然最后别忘了对自旋锁 sem_lock 进行解锁
 */

/**
 * 信号量初始化
 */
void sem_init(sem_t * sem, int init_count)
{
    sem->count = init_count;
    list_init(&sem->wait_list);
}

/**
 * 申请信号量
 * 信号量的实现原理可类比停车场前面的计数牌
 * 停车场前面会有一个计数牌, 上面显示当前停车场内空闲车位数
 * 1) 车子进来时
 *    当计数牌数>0, 说明有空闲车位, 车可直接开进去, 计数牌数减1
 *    当计数牌数==0, 说明没有空闲车位, 将当前车放入"车辆等待队列"末尾
 * 2) 车子出去时
 *    若"车辆等待队列"中有车子在等待, 则将"车辆等待队列"第一辆车取出来, 并让其进入停车场
 *    若"车辆等待队列"中没有车子在等待, 计数牌数加1
 */
void sem_wait(sem_t * sem)
{
    irq_state_t irq_state = irq_enter_protection();

    // 1. 计数器>0, 说明有空闲位, 直接继续运行, 计数器-1
    if (sem->count > 0)
    {
        sem->count--;
    }
    // 2. 计数器==0, 明没有空闲位, 将当前进程从就绪队列中移除, 并放入等待队列末尾, 最后调度切换出去
    else
    {
        // 先获取当前任务
        task_t * curr = task_current();
        // 从就绪队列中移除
        task_set_block(curr);
        // 然后加入信号量的等待队列
        list_insert_last(&sem->wait_list, &curr->wait_node);
        // 当前进程进入等待睡眠状态, 需从新进行进程调度
        task_dispatch();
    }

    irq_leave_protection(irq_state);
}

/**
 * 释放信号量
 */
void sem_notify(sem_t * sem)
{
    irq_state_t  irq_state = irq_enter_protection();

    // 1. 等待队列中有进程, 将等待队列中第一个进程移除, 并放入就绪队列末尾, 最后调度切换出去
    if (list_count(&sem->wait_list))
    {
        // 有进程等待时, 先将它从等待队列中移除
        list_node_t * node = list_remove_first(&sem->wait_list);
        // 获取当前任务节点
        task_t * task = list_node_parent(node, task_t, wait_node);
        // 将任务唤醒并加入就绪队列
        task_set_ready(task);

        // 进入就绪队列后需从新进行进程调度
        task_dispatch();
    }
    // 2. 等待队列中没有进程, 计数器+1后直接继续运行
    else
    {
        sem->count++;
    }

    irq_leave_protection(irq_state);
}

/**
 * 获取信号量的当前值
 */
int sem_count(sem_t * sem)
{
    irq_state_t  irq_state = irq_enter_protection();
    int count = sem->count;
    irq_leave_protection(irq_state);
    return count;
}


#if 0
#define SEM_FLG_MUTEX            0
#define SEM_FLG_MULTI            1
#define SEM_MUTEX_ONE_LOCK       1
#define SEM_MULTI_LOCK           0

/**
 * 等待链数据结构, 用于挂载等待代码执行流（线程）的结构, 里面有用于挂载代码执行流的链表和计数器变量
 */
typedef struct s_KWLST
{   
    spinlock_t wl_lock;
    uint32_t   wl_tdnr;
    list_h_t   wl_list;
} kernel_wait_list_t;

// 信号量数据结构
typedef struct s_KSEM
{
    spinlock_t sem_lock; // 维护sem_t自身数据的自旋锁
    uint32_t   sem_flg;      // 信号量相关的标志
    int        sem_count;    // 信号量计数值
    kernel_wait_list_t sem_waitlst;  //用于挂载等待代码执行流（线程）结构
} kernel_sem_t;


// 获取信号量
void kernel_sem_down(kernel_sem_t * sem)
{
    cpuflg_t cpufg;

start_step:
    kernel_spin_lock_sti(&sem->sem_lock, &cpufg);
    if(sem->sem_count < 1)
    {
        // 如果信号量值小于1,则让代码执行流（线程）睡眠
        kernel_wait_list_wait(&sem->sem_waitlst);
        kernel_spin_unlock_sti(&sem->sem_lock, &cpufg);
        kernel_schedule();  //切换代码执行流，下次恢复执行时依然从下一行开始执行，所以要goto开始处重新获取信号量
        goto start_step;
    }

    sem->sem_count--;  //信号量值减1, 表示成功获取信号量
    kernel_spin_unlock_sti(&sem->sem_lock, &cpufg);
}

//释放信号量
void kernel_sem_up(kernel_sem_t * sem)
{
    cpuflg_t cpufg;

    kernel_spin_lock_sti(&sem->sem_lock, &cpufg);
    sem->sem_count++;  // 释放信号量
    if(sem->sem_count < 1)
    {
        //如果小于1,则说数据结构出错了，挂起系统
        kernel_spin_unlock_sti(&sem->sem_lock, &cpufg);
        sys_die("sem up err");
    }

    // 唤醒该信号量上所有等待的代码执行流（线程）
    kernel_wait_list_allup(&sem->sem_waitlst);
    kernel_spin_unlock_sti(&sem->sem_lock, &cpufg);

    kernel_set_schedule_flags();
}
#endif

