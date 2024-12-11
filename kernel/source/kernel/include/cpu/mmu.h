/**
 * 手写操作系统
 *
 * 文件名: mmu.h
 * 功  能: MMU与分布处理
 */

#ifndef MMU_H
#define MMU_H

#include "comm/types.h"
#include "comm/cpu_instr.h"


#define PDE_CNT             (1024)
#define PTE_CNT             (1024)
#define PTE_P               (1 << 0)
#define PDE_P               (1 << 0)
#define PTE_W               (1 << 1)
#define PTE_R               (0 << 1)
#define PDE_W               (1 << 1)
#define PTE_U               (1 << 2)
#define PDE_U               (1 << 2)


#pragma pack(1)
/**
 * @brief PDE(Page-Directory Entry)
 */
typedef union _pde_t
{
    uint32_t v;
    struct
    {
        uint32_t present : 1;                   // 0 (P) Present; must be 1 to map a 4-KByte page
        uint32_t write_disable : 1;             // 1 (R/W) Read/write, if 0, writes may not be allowed
        uint32_t user_mode_acc : 1;             // 2 (U/S) if 0, user-mode accesses are not allowed
        uint32_t write_through : 1;             // 3 (PWT) Page-level Write-Through
        uint32_t cache_disable : 1;             // 4 (PCD) Page-level Cache Disable
        uint32_t accessed : 1;                  // 5 (A) Accessed
        uint32_t ignored1 : 1;                  // 6 Ignored
        uint32_t ps : 1;                        // 7 (PS) Page Size
        uint32_t ignored2 : 4;                  // 11:8 Ignored
        uint32_t phy_pt_addr : 20;              // 高20位page table物理地址
    };
} pde_t;

/**
 * @brief PTE(Page-Table Entry)
 */
typedef union _pte_t
{
    uint32_t v;
    struct
    {
        uint32_t present : 1;                   // 0 (P) Present; must be 1 to map a 4-KByte page
        uint32_t write_disable : 1;             // 1 (R/W) Read/write, if 0, writes may not be allowed
        uint32_t user_mode_acc : 1;             // 2 (U/S) if 0, user-mode accesses are not allowed
        uint32_t write_through : 1;             // 3 (PWT) Page-level write-through
        uint32_t cache_disable : 1;             // 4 (PCD) Page-level cache disable
        uint32_t accessed : 1;                  // 5 (A) Accessed
        uint32_t dirty : 1;                     // 6 (D) Dirty
        uint32_t pat : 1;                       // 7 (PAT)
        uint32_t global : 1;                    // 8 (G) Global
        uint32_t Ignored : 3;                   // Ignored
        uint32_t phy_page_addr : 20;            // 高20位物理地址
    };
} pte_t;

#pragma pack()

/**
 * @brief 返回vaddr在页目录中的索引
 */
static inline uint32_t pde_index(uint32_t vaddr)
{
    return (vaddr >> 22) & 0x3FF; // 只取高10位
}

/**
 * @brief 获取pde中地址
 */
static inline uint32_t pde_paddr(pde_t * pde)
{
    return pde->phy_pt_addr << 12;
}

/**
 * @brief 返回vaddr在页表中的索引
 */
static inline int pte_index(uint32_t vaddr)
{
    return (vaddr >> 12) & 0x3FF;   // 取中间10位
}

/**
 * @brief 获取pte中的物理地址
 */
static inline uint32_t pte_paddr(pte_t * pte)
{
    return pte->phy_page_addr << 12;
}

/**
 * @brief 获取pte中的权限位
 */
static inline uint32_t get_pte_perm(pte_t * pte)
{
    return (pte->v & 0x1FF);
}

/**
 * @brief 将页目录基地址写入CR3寄存器
 * @param vaddr 页表的虚拟地址
 */
static inline void mmu_set_page_dir(uint32_t paddr)
{
    // 将页目录表基地址写入CR3寄存器
    write_cr3((uint32_t)paddr);
}


#endif // MMU_H
