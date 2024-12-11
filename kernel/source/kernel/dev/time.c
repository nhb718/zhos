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


