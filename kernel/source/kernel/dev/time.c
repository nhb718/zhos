/**
 * 手写操作系统
 *
 * 文件名: timer.c
 * 功  能: 定时器处理
 */

#include "dev/time.h"
#include "cpu/irq.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "core/task.h"


#if 0
// 下面是关于定时器的代码，最多可有64个定时器。
#define TIME_REQUESTS    64

// 定时器链表结构和定时器数组。该定时器链表专用于供软驱关闭马达和启动马达定时操作。
// 这种类型定时器类似现代Linux系统中的动态定时器(Dynamic Timer)，仅供内核使用。
static struct timer_list_t
{
    long jiffies;                   // 定时滴答数
    void (*fn)();                   // 定时处理程序
    struct timer_list_t * next;       // 链接指向下一个定时器
} timer_list[TIME_REQUESTS], * next_timer = nullptr;   // next_timer是定时器队列头指针
#endif


static uint32_t sys_tick;    // 系统启动后的tick数量



static void pit_init(void)
{
    uint32_t reload_count = PIT_OSC_FREQ / (1000.0 / OS_TICK_MS); // 此处设置每隔10ms产生一次中断

    // 按照8253定时器芯片手册中的说明初始化定时器硬件参数寄存器, 使用模式3
    // 定时器芯片一般有3个定时器, 这里只使用到定时器0 Timer_0
    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE3);

    /**
     * 加载16位的初始值
     * 定时器芯片工作原理
     * 1) 使用reload_count初始化芯片中的计数器寄存器, 随着时间的推移 定时器芯片中的计数器不断减少
     * 2) 当 计数器 减少到0时就会触发中断通知中断控制器
     * 3) 同时定时器芯片中的硬件模块会重新加载reload_count到计数器寄存器, 这样就形成周期性中断的定时器
     */
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF);        // 先加载低8位
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF); // 再加载高8位
}

/**
 * 定时器中断处理函数
 */
void do_handler_timer(exception_frame_t *frame)
{
    sys_tick++;

    // 先发EOI清除中断, 而不是放在最后
    // 放最后将从任务中切换出去之后，除非任务再切换回来才能继续噢应
    pic_send_eoi(IRQ0_TIMER);

    task_time_tick(); // 每10ms系统心跳时都会进行进程管理: 执行时间片轮转调度算法

    #if 0
    /**
     * 定时器时间到, 调用定时器超时函数
     * 每个timer的jiffies保存的是两个定时器之间的差额值
     * 当第一个定时器超时调用超时函数后, 第二个代替第一个定时器成为next_timer
     */
    if (next_timer)
    {
        next_timer->jiffies--;
        while (next_timer && next_timer->jiffies <= 0)
        {
            typedef void (*callback_fn)(void);
            callback_fn fn = next_timer->fn;
            next_timer->fn = nullptr;
            next_timer = next_timer->next;
            fn();
        }
    }
    #endif

}

/**
 * 定时器初始化
 * 定时器芯片8253参考资料
 * 8253定时器芯片的资料: https://wiki.osdev.org/Programmable_Interval_Timer
 * 关于8253配置文档:  http://www.osdever.net/bkerndev/Docs/pit.htm
 * 相关文档:  ../documents/8253.pd
 * 相关书籍:  《Linux内核完全剖析  第316页》
 */
void time_init(void)
{
    sys_tick = 0; // 系统心跳数, 该参数在中断处理函数中加1

    // 定时器芯片寄存器初始化
    pit_init();

    /**
     * 注册定时器的中断处理函数
     * exception_handler_timer 在start.S 中定义
     * 1) 保存中断上下文
     * 2) 执行 do_handler_timer, 该函数做如下两步
     *   2.1) 清中断
     *   2.2) 进行进程管理, 执行时间片轮转调度算法
     * 3) 恢复中断上下文
     */
    irq_install(IRQ0_TIMER, (irq_handler_t)exception_handler_timer);
    irq_enable(IRQ0_TIMER);
}


#if 0
// 添加定时器。输入参数为指定的定时值(滴答数)和相应的处理程序指针。
// 软盘驱动程序(floppy.c)利用该函数执行启动或关闭马达的延时操作。
// 参数jiffies - 以10毫秒计的滴答数：*fn() - 定时时间到时执行的函数
int add_timer(long jiffies, void (*fn)(void))
{
    struct timer_list_t * p;
    int timer_id = -1;

    // 如果定时处理程序指针为空，则退出
    if (!fn)
        return timer_id;

    irq_disable_global();
    // 如果定时值 <= 0, 则立刻调用其处理程序, 并且该定时器不加入链表中
    if (jiffies <= 0)
    {
        (fn)();
    }
    else
    {
        // 否则从定时器数组中，找一个空闲项。
        for (int i=0; i < TIME_REQUESTS; i++)
        {
            p = &timer_list[i];
            if (!p->fn)
            {
                timer_id = i;
                break;
            }
        }

        // 如果已经用完了定时器数组，则系统崩溃;-).
        if (timer_id == -1)
        {
            irq_enable_global();
            return timer_id;
        }

        // 向定时器数据结构填入相应信息, 并链入链表头
        p->fn = fn;
        p->jiffies = jiffies;
        p->next = next_timer;
        next_timer = p;

        // 链表项按定时值从小到大排序。在排序时减去排在前面需要的滴答数，这样在
        // 处理定时器时只要查看链表头的第一项的定时是否到期即可。[[ 这段程序没有
        // 考虑周全。如果新插入的定时器值小于原来头一个定时器值时根本不会进入循环中，
        // 但此时还是应该将紧随其后面的一个定时器值减去新的第一个定时值。即如果
        // 第1个定时值<=第2个，则第2个定时值扣除第1个的值即可，否则进入下面循环中进行处理]]
        if (p->next && p->jiffies < p->next->jiffies)
        {
            // 链表项按定时值从小到大已排序, 只需将p->next的jiffies减去p的jiffies
            p->next->jiffies -= p->jiffies;
        }
        else
        {
            while (p->next && p->next->jiffies < p->jiffies)
            {
                // p的jiffies要减去p->next的jiffies
                p->jiffies -= p->next->jiffies;
                // 交换p和p->next的fn
                fn = p->fn;
                p->fn = p->next->fn;
                p->next->fn = fn;
                // 交换p和p->next的jiffies
                jiffies = p->jiffies;
                p->jiffies = p->next->jiffies;
                p->next->jiffies = jiffies;
                // p来到了p->next
                p = p->next;
            }
        }
    }
    irq_enable_global();
    return timer_id;
}


void del_timer(int timer_id)
{
}
#endif

