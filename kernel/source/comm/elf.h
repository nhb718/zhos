/**
 * 手写操作系统
 *
 * 文件名: elf.h
 * 功  能: ELF相关头文件及配置
 */

#ifndef OS_ELF_H
#define OS_ELF_H

#include "types.h"

// ELF相关数据类型
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

// ELF Header
#define EI_NIDENT       16
#define ELF_MAGIC       0x7F

#define ET_EXEC         2   // 可执行文件
#define ET_386          3   // 80386处理器

#define PT_LOAD         1   // 可加载类型

#pragma pack(1) // 1字节对齐, 禁止填充, 防止使用默认的4字节对齐, 导致数据在内存中布局不正确
typedef struct
{
    char e_ident[EI_NIDENT]; // 类似魔数, 识别是否是ELF文件, 前4字节是".ELF", Magic number and other info
    Elf32_Half e_type;       /* Object file type */
    Elf32_Half e_machine;    /* Architecture */
    Elf32_Word e_version;    /* Object file version */
    Elf32_Addr e_entry;      // 保存elf程序的入口地址, Entry point virtual address
    Elf32_Off  e_phoff;      // 段偏移量, 用以索引段的起始地址, Program header table file offset
    Elf32_Off  e_shoff;      /* Section header table file offset */
    Elf32_Word e_flags;      /* Processor-specific flags */
    Elf32_Half e_ehsize;     /* ELF header size in bytes */
    Elf32_Half e_phentsize;  /* Program header table entry size */
    Elf32_Half e_phnum;      /* Program header table entry count */
    Elf32_Half e_shentsize;  /* Section header table entry size */
    Elf32_Half e_shnum;      /* Section header table entry count */
    Elf32_Half e_shstrndx;   /* Section header string table index */
}Elf32_Ehdr;

#define PT_LOAD         1    // 1 - 可被加载的 Segment

typedef struct
{
    Elf32_Word p_type;       /* Segment type, 只有当为1时才可被加载的Segment */
    Elf32_Off  p_offset;     /* Segment file offset */
    Elf32_Addr p_vaddr;      /* Segment virtual address */
    Elf32_Addr p_paddr;      /* Segment physical address */
    Elf32_Word p_filesz;     /* Segment size in file */
    Elf32_Word p_memsz;      /* Segment size in memory */
    Elf32_Word p_flags;      /* Segment flags */
    Elf32_Word p_align;      /* Segment alignment */
} Elf32_Phdr;

#pragma pack()

#endif //OS_ELF_H
