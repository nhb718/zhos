/**
 * 手写操作系统
 *
 * 文件名: mutex.c
 * 功  能: 互斥锁
 */

#include "cpu/irq.h"
#include "ipc/mutex.h"


/**
 * 锁初始化
 */
void mutex_init(mutex_t * mutex)
{
    mutex->locked_count = 0;
    mutex->owner = (task_t *)0;
    list_init(&mutex->wait_list);
}

/**
 * 申请锁
 */
void mutex_lock(mutex_t * mutex)
{
    irq_state_t irq_state = irq_enter_protection();

    // 获取当前任务task_t, 确定当前上锁操作的owner
    task_t * curr = task_current();
    // 1. 没有进程拥有该锁, 将count+1, 当前任务占用之
    if (mutex->locked_count == 0)
    {
        mutex->locked_count++;
        mutex->owner = curr; // owner为当前任务
    }
    // 2. 已有进程占用, 判断一下owner是否为当前任务, 若是则简单将count+1
    else if (mutex->owner == curr)
    {
        // 该锁已为当前任务所拥有, 只增加计数
        mutex->locked_count++;
    }
    // 3. 已有进程占用, 且不是当前任务, 将当前任务从就绪队列移除, 并放入等待队列末尾, 最后调度切换出去
    else
    {
        // 该锁已被其它任务占用, 则将当前进程扔进等待队列中
        task_set_block(curr);
        list_insert_last(&mutex->wait_list, &curr->wait_node);
        task_dispatch();
    }

    irq_leave_protection(irq_state);
}

/**
 * 释放锁
 */
void mutex_unlock(mutex_t * mutex)
{
    irq_state_t irq_state = irq_enter_protection();

    task_t * curr = task_current();
    // 只有锁的拥有者才能释放锁
    if (mutex->owner == curr)
    {
        // 锁已经释放, 查看当前等待队列中
        if (--mutex->locked_count == 0)
        {
            // 减到0，释放锁
            mutex->owner = (task_t *)0;

            // 如果等待队列中有任务等待，则立即唤醒并占用锁
            if (list_count(&mutex->wait_list))
            {
                list_node_t * task_node = list_remove_first(&mutex->wait_list);
                task_t * task = list_node_parent(task_node, task_t, wait_node);
                task_set_ready(task);

                // 在这里占用，而不是在任务醒后占用，因为可能抢不到
                mutex->locked_count = 1;
                mutex->owner = task;

                task_dispatch();
            }
        }
    }

    irq_leave_protection(irq_state);
}




