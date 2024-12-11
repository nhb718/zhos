/**
 * 手写操作系统
 *
 * 文件名: sem.h
 * 功  能: 计数信号量
 */

#ifndef OS_SEM_H
#define OS_SEM_H


#include "tools/list.h"
#include "spinlock.h"


/**
 * 进程同步用的计数信号量
 */
typedef struct _sem_t
{
    //spinlock_t sem_lock;     // 维护sem_t自身数据的自旋锁
    int count;               // 信号量计数
    list_t wait_list;        // 等待的进程列表
}sem_t;


void sem_init(sem_t * sem, int init_count);
void sem_wait(sem_t * sem);
void sem_notify(sem_t * sem);
int sem_count(sem_t * sem);


#endif //OS_SEM_H
