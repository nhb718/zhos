/**
 * 手写操作系统
 *
 * 文件名: atomic.h
 * 功  能: 原子锁
 * 注意: 原子操作锁只适合于单体变量, 如整数
 */

#ifndef ATOMIC_H
#define ATOMIC_H


// 定义一个原子类型
typedef struct s_ATOMIC
{
    volatile int a_count; //在变量前加上volatile，是为了禁止编译器优化，使其每次都从内存中加载变量
} atomic_t;


// 原子读
int atomic_read(const atomic_t * v);

// 原子写
void atomic_write(atomic_t * v, int i);

// 原子加上一个整数
void atomic_add(int i, atomic_t * v);

// 原子减去一个整数
void atomic_sub(int i, atomic_t * v);

// 原子加1
void atomic_inc(atomic_t * v);

// 原子减1
void atomic_dec(atomic_t * v);


#endif // ATOMIC_H
