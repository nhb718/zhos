/**
 * 手写操作系统
 *
 * 文件名: spinlock.h
 * 功  能: 自旋锁
 */

#ifndef SPIN_LOCK_H
#define SPIN_LOCK_H


typedef unsigned int cpuflg_t;

// 自旋锁结构
typedef struct
{
    // volatile可以防止编译器优化, 保证其它代码始终从内存加载lock变量的值 
    volatile unsigned int lock;
} spinlock_t;


void spin_lock_init(spinlock_t * lock);
void spin_lock(spinlock_t * lock);
void spin_unlock(spinlock_t * lock);
void spin_lock_disable_irq(spinlock_t * lock, cpuflg_t * flags);
void spin_unlock_enabled_irq(spinlock_t * lock, cpuflg_t * flags);


#endif // SPIN_LOCK_H
