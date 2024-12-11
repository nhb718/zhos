/**
 * 手写操作系统
 *
 * 文件名: tty.c
 * 功  能: 终端tty
 * 目前只考虑处理cooked模式的处理
 */

#include "dev/tty.h"
#include "dev/console.h"
#include "dev/kbd.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "cpu/irq.h"


static tty_t tty_devs[TTY_NR];
static int curr_tty = 0;

/**
 * @brief FIFO初始化
 */
static void tty_fifo_init(tty_fifo_t * fifo, char * buf, int size)
{
    fifo->buf   = buf;
    fifo->count = 0;
    fifo->size  = size;
    fifo->read  = 0;
	fifo->write = 0;
}

/**
 * @brief 判断tty是否有效
 */
static inline tty_t * get_tty(device_t * dev)
{
    int tty = dev->minor;
    if ((tty < 0) || (tty >= TTY_NR) || (!dev->open_count))
    {
        log_printf("tty is not opened. tty = %d", tty);
        return (tty_t *)0;
    }

    return tty_devs + tty;
}

/**
 * @brief 从fifo缓存中取一字节数据
 */
int tty_fifo_get(tty_fifo_t * fifo, char * c)
{
	// 判断当前fifo是否有数据
    if (fifo->count <= 0)
    {
        return -1;
    }

    irq_state_t state = irq_enter_protection();
    *c = fifo->buf[fifo->read++];  // 从缓存中读1字节
    if (fifo->read >= fifo->size)  // 读到最末尾时也重新从0开始
    {
        fifo->read = 0;
    }
    fifo->count--;
    irq_leave_protection(state);
    return 0;
}

/**
 * @brief 往fifo缓存中写一字节数据
 */
int tty_fifo_put(tty_fifo_t * fifo, char c)
{
	// 判断当前fifo是否还有剩余空间
    if (fifo->count >= fifo->size)
    {
        return -1;
    }

    irq_state_t state = irq_enter_protection();
    fifo->buf[fifo->write++] = c;  // 将1字节写入缓存中
    if (fifo->write >= fifo->size) // 写到最末尾时重新从0开始
    {
        fifo->write = 0;
    }
    fifo->count++;
    irq_leave_protection(state);

    return 0;
}

/**
 * @brief 打开tty设备
 */
int tty_open(device_t * dev)
{
    int idx = dev->minor; // 获取次设备号, 确认打开的是哪个tty(0-7)
    if ((idx < 0) || (idx >= TTY_NR))
    {
        log_printf("open tty failed. incorrect tty num = %d", idx);
        return -1;
    }

    tty_t * tty = tty_devs + idx;
	// 初始化输入输出缓存队列
    tty_fifo_init(&tty->ofifo, tty->obuf, TTY_OBUF_SIZE);
    sem_init(&tty->osem, TTY_OBUF_SIZE);
    tty_fifo_init(&tty->ififo, tty->ibuf, TTY_IBUF_SIZE);
    sem_init(&tty->isem, 0);

    tty->console_idx = idx; // 保存次设备号
    tty->iflags = TTY_INLCR | TTY_IECHO;
    tty->oflags = TTY_OCRLF;

    kbd_init(); // 键盘初始化
    console_init(idx); // 控制台初始化
    return 0;
}


/**
 * @brief 向tty写入数据
 */
int tty_write(device_t * dev, int addr, char * buf, int size)
{
    if (size < 0)
    {
        return -1;
    }

    tty_t * tty = get_tty(dev);
    int len = 0;

    // 先将所有数据写入缓存中
    while (size)
    {
        char c = *buf++;

        // 如果遇到\n，根据配置决定是否转换成\r\n
        if (c == '\n' && (tty->oflags & TTY_OCRLF))
        {
            sem_wait(&tty->osem);
            int err = tty_fifo_put(&tty->ofifo, '\r');
            if (err < 0)
            {
                break;
            }
        }

        // 写入当前字符
        sem_wait(&tty->osem);
        int err = tty_fifo_put(&tty->ofifo, c);
        if (err < 0)
        {
            break;
        }

        len++;
        size--;

        // 启动输出, 这里是直接由console直接输出，无需中断
        console_write(tty);
    }

    return len;
}

/**
 * @brief 从tty读取数据
 */
int tty_read(device_t * dev, int addr, char * buf, int size)
{
    if (size < 0)
    {
        return -1;
    }

    tty_t * tty = get_tty(dev);
    char * pbuf = buf;
    int len = 0;

    // 不断读取，直到遇到文件结束符或者行结束符
    while (len < size)
    {
        // 等待可用的数据
        sem_wait(&tty->isem);

        // 取出数据
        char ch;
        tty_fifo_get(&tty->ififo, &ch);
        switch (ch)
        {
            case ASCII_DEL:
                if (len == 0)
                {
                    continue;
                }
                len--;
                pbuf--;
                break;
            case '\n':
			    // 遇到回车符'\n'时, 需判断是否将\n转成\r\n
                if ((tty->iflags & TTY_INLCR) && (len < size - 1))
                {
                    // \n变成\r\n
                    *pbuf++ = '\r';
                    len++;
                }
                *pbuf++ = '\n';
                len++;
                break;
            default:
                *pbuf++ = ch;
                len++;
                break;
        }

        if (tty->iflags & TTY_IECHO)
        {
            tty_write(dev, 0, &ch, 1);
        }

        // 遇到一行结束，也直接跳出
        if ((ch == '\r') || (ch == '\n'))
        {
            break;
        }
    }

    return len;
}

/**
 * @brief 向tty设备发送命令
 */
int tty_control(device_t * dev, int cmd, int arg0, int arg1)
{
    tty_t * tty = get_tty(dev);

    switch (cmd)
    {
    case TTY_CMD_ECHO:
        if (arg0)
        {
            tty->iflags |= TTY_IECHO;
            console_set_cursor(tty->console_idx, 1);
        }
        else
        {
            tty->iflags &= ~TTY_IECHO;
            console_set_cursor(tty->console_idx, 0);
        }
        break;
    case TTY_CMD_IN_COUNT:
        if (arg0)
        {
            *(int *)arg0 = sem_count(&tty->isem);
        }
        break;
    default:
        break;
    }
    return 0;
}

/**
 * @brief 关闭tty设备
 */
void tty_close(device_t * dev)
{

}

/**
 * @brief 输入tty字符
 */
void tty_in(char ch)
{
    tty_t * tty = tty_devs + curr_tty;

    // 辅助队列要有空闲空间可代写入
    if (sem_count(&tty->isem) >= TTY_IBUF_SIZE)
    {
        return;
    }

    // 写入辅助队列，通知数据到达
    tty_fifo_put(&tty->ififo, ch);
    sem_notify(&tty->isem);
}

/**
 * @brief 选择tty
 */
void tty_select(int tty)
{
    if (tty != curr_tty)
    {
        console_select(tty);
        curr_tty = tty;
    }
}

// 设备描述表: 描述一个设备所具备的特性
dev_desc_t dev_tty_desc =
{
    .name = "tty",
    .major = DEV_TTY,
    .open = tty_open,
    .read = tty_read,
    .write = tty_write,
    .control = tty_control,
    .close = tty_close,
};
