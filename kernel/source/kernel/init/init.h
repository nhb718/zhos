/**
 * 手写操作系统
 *
 * 文件名: init.h
 * 功能描述: 内核初始化代码
 */

#ifndef __INIT_H__
#define __INIT_H__

#include "comm/boot_info.h"


void kernel_init(boot_info_t * boot_info);


#endif // INIT_H