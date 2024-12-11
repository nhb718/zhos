/**
 * 手写操作系统
 *
 * 文件名: mutex.h
 * 功  能: 互斥锁
 */

#ifndef MUTEX_H
#define MUTEX_H


#include "core/task.h"
#include "tools/list.h"


/**
 * 进程同步用的计数信号量
 */
typedef struct _mutex_t
{
    task_t * owner;   // 锁的拥有进程
    int locked_count; // 上锁次数计数器
    list_t wait_list; // 等待队列
} mutex_t;


void mutex_init(mutex_t * mutex);
void mutex_lock(mutex_t * mutex);
void mutex_unlock(mutex_t * mutex);
 

#endif //MUTEX_H
