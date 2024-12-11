
#include "ipc/spinlock.h"


/**
 * 自旋锁
 * 适用场景: 解决多CPU核情况下共享临界资源的解决方案
 *
 * 自旋锁的原理
 * 1) 首先读取锁变量, 判断其值是否已经加锁
 *    这里必须保证读取锁变量和判断并加锁的操作是原子执行的
 *    x86 CPU 提供一个原子交换指令: xchg, 它可以让寄存器里的一个值跟内存空间中的一个值做交换
 * 2) 如果未加锁则执行加锁, 然后返回, 表示加锁成功
 * 2) 如果已经加锁了, 就要返回第一步继续执行后续步骤
 */

// 自旋锁初始化函数
void spin_lock_init(spinlock_t * lock)
{
    // 锁值初始化为0是未加锁状态
    lock->lock = 0;
}

// 加锁函数
void spin_lock(spinlock_t * lock)
{
    __asm__ __volatile__(
                    "1: \n"
                    "lock; xchg  %0, %1 \n" // 把值为1的寄存器和lock内存中的值进行交换
                    "cmpl   $0, %0 \n"      // 用0和交换回来的值进行比较
                    "jnz    2f \n"          // 不等于0则跳转后面2标号处运行
                    "jmp 3f \n"             // 若等于0则跳转后面3标号处返回
                    "2:         \n" 
                    "cmpl   $0, %1  \n"     // 用0和lock内存中的值进行比较
                    "jne    2b      \n"     // 若不等于0则跳转到前面2标号处运行继续比较  
                    "jmp    1b      \n"     // 若等于0则跳转到前面1标号处运行, 交换并加锁
                    "3:  \n"     :
                    : "r"(1), "m"(*lock));
}

// 解锁函数
void spin_unlock(spinlock_t * lock)
{
    __asm__ __volatile__(
                    "movl   $0, %0\n"       // 解锁把lock内存中的值设为0就行
                    :
                    : "m"(*lock));
}

// 关中断下的自旋锁
void spin_lock_disable_irq(spinlock_t * lock, cpuflg_t * flags)
{
    __asm__ __volatile__(
                    "pushf            \n\t"
                    "cli              \n\t"
                    "pop %0           \n\t"
                    "1:               \n\t"
                    "lock; xchg  %1, %2 \n\t"
                    "cmpl   $0,%1     \n\t"
                    "jnz    2f        \n\t"
                    "jmp    3f        \n"  
                    "2:     \n\t"
                    "cmpl   $0,%2     \n\t" 
                    "jne    2b        \n\t"
                    "jmp    1b        \n\t"
                    "3:     \n"     
                    :"=m"(*flags)
                    : "r"(1), "m"(*lock));
}

// 开中断下的自旋锁
void spin_unlock_enabled_irq(spinlock_t * lock, cpuflg_t * flags)
{
    __asm__ __volatile__(
                    "movl   $0, %0 \n\t"
                    "push %1 \n\t"
                    "popf \n\t"
                    :
                    : "m"(*lock), "m"(*flags));
}

