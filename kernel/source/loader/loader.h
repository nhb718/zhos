/**
 * 手写操作系统
 *
 * 文件名: loader.h
 * 功  能: 二级加载部分, 用于实现更为复杂的初始化、内核加载的工作
 */

#ifndef LOADER_H
#define LOADER_H

#include "comm/types.h"
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"


extern boot_info_t boot_info; // 启动参数信息, boot在加载loader程序时传入

// 保护模式入口函数, 在start.S中定义
extern void protect_mode_entry(void);


// 内存检测信息结构
typedef struct SMAP_entry
{
    uint32_t BaseL; // 64bits base address High_32:low_32
    uint32_t BaseH;
    uint32_t LengthL; // length uint64_t
    uint32_t LengthH;
    uint32_t Type; // entry type, 值为1时表明为我们可用的RAM空间
    uint32_t ACPI; // extended, bit0=0时表明此条目应当被忽略
}__attribute__((packed)) SMAP_entry_t;


void loader_entry(void);


#endif // LOADER_H
