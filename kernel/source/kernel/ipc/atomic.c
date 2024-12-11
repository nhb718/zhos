
#include "ipc/atomic.h"


/**
 * 原子操作
 * 适用场景: 只适用于基本数据类型, 如 int, short 等
 *
 * 原子操作, 顾名思义, 操作是原子不可分隔的, 也就是说要 a++ 这个操作不可分隔, 即 a++ 要么不执行, 要么一口气执行完
 */


// 原子读
int atomic_read(const atomic_t * v)
{        
    //x86平台取地址处是原子
    return (*(volatile int*)&(v)->a_count);
}

// 原子写
void atomic_write(atomic_t * v, int i)
{
    //x86平台把一个值写入一个地址处也是原子的 
    v->a_count = i;
}

/**
 * 嵌入式汇编代码语法说明如下:
 * __asm__ __volatile__(汇编代码部分 : 输出部分列表 : 输入部分列表 : 损坏部分列表);
 *
 * 1) 汇编代码部分, 这里是实际嵌入的汇编代码
 * 2) 输出列表部分, 让 GCC 能够处理 C 语言左值表达式与汇编代码的结合
 * 3) 输入列表部分, 也是让 GCC 能够处理 C 语言表达式、变量、常量, 让它们能够输入到汇编代码中去
 * 4) 损坏列表部分, 告诉 GCC 汇编代码中用到了哪些寄存器, 以便 GCC 在汇编代码运行前, 生成保存它们的代码
 *    并且在生成的汇编代码运行后，恢复它们（寄存器）的代码
 * 
 * 它们之间用冒号隔开，如果只有汇编代码部分，后面的冒号可以省略
 * 但是有输入列表部分而没有输出列表部分的时候，输出列表部分的冒号就必须要写，否则 GCC 没办法判断，同样的道理对于其它部分也一样
 */

/**
 * 原子操作的关键指令: lock 前缀表示锁定总线, 这样加上 lock 前缀的 addl、subl、incl、decl 指令都是原子操作
 *
 * 原子加上一个整数
 * "lock;" "addl %1,%0" 是汇编指令部分，%1,%0是占位符，它表示输出、输入列表中变量或表态式
 *   占位符的数字从输出部分开始依次增加，这些变量或者表态式会被GCC处理成寄存器、内存、立即数放在指令中。 
 * : "+m" (v->a_count) 是输出列表部分，“+m”表示(v->a_count)和内存地址关联
 * : "ir" (i) 是输入列表部分，“ir” 表示i是和立即数或者寄存器关联
 */
void atomic_add(int i, atomic_t * v)
{
    __asm__ __volatile__("lock;" "addl %1,%0" // lock 前缀表示锁定总线
                    : "+m" (v->a_count)
                    : "ir" (i));
}

// 原子减去一个整数
void atomic_sub(int i, atomic_t * v)
{
    __asm__ __volatile__("lock;" "subl %1,%0" // lock 前缀表示锁定总线
                    : "+m" (v->a_count)
                    : "ir" (i));
}

//原子加1
void atomic_inc(atomic_t * v)
{
    __asm__ __volatile__("lock;" "incl %0"    // lock 前缀表示锁定总线
                    : "+m" (v->a_count));
}

// 原子减1
void atomic_dec(atomic_t * v)
{
    __asm__ __volatile__("lock;" "decl %0"    // lock 前缀表示锁定总线
                    : "+m" (v->a_count));
}


