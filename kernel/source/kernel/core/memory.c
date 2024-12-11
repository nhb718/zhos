/**
 * 手写操作系统
 *
 * 文件名: memory.c
 * 功  能: 内存管理
 */

#include "tools/klib.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"
#include "cpu/mmu.h"
#include "dev/console.h"


/**
 * 内存管理模块只是简单的分配、释放以页为单位的物理内存, 通过bitmap来管理空闲/占用标记
 * 并没有将非连续的物理内存页统一进行管理
 */

static addr_alloc_t paddr_alloc;        // 物理地址的分配结构
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // 内核页目录表


/**
 * @brief 获取当前进程的页目录项地址, 即当前进程中tss段CR3寄存器的值
 */
static pde_t * current_page_dir(void)
{
    return (pde_t *)task_current()->tss.cr3;
}

/**
 * @brief 初始化地址分配结构
 * addr_alloc_t * alloc  管理物理内存分配的结构体
 * uint8_t * bits  bitmap起始物理地址
 * uint32_t start  申请内存的起始地址
 * uint32_t size   申请内存的地址大小
 * uint32_t page_size  每一页物理内存的大小
 * 函数内不检查start和size的页边界, 由上层调用者检查
 */
static void addr_alloc_init(addr_alloc_t * alloc, uint8_t * bits,
                    uint32_t start, uint32_t size, uint32_t page_size)
{
    mutex_init(&alloc->mutex);

    alloc->start = start;
    alloc->size  = size;
    alloc->page_size = page_size;
    // alloc->size / page_size - 内存总大小/页的大小 = 一共有多少物理页, 每一物理页用一个bit位来表示
    bitmap_init(&alloc->bitmap, bits, alloc->size / page_size, 0);
}

/**
 * @brief 分配多个页的物理内存, 默认一页=4KB
 */
static uint32_t addr_alloc_page(addr_alloc_t * alloc, int page_count)
{
    uint32_t addr = 0;

    mutex_lock(&alloc->mutex);

    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if (page_index >= 0)
    {
        addr = alloc->start + page_index * alloc->page_size;
    }

    mutex_unlock(&alloc->mutex);
    return addr;
}

/**
 * @brief 释放多页内存
 */
static void addr_free_page(addr_alloc_t * alloc, uint32_t addr, int page_count)
{
    mutex_lock(&alloc->mutex);

    // (addr- base_addr) / page_size = page_index, (待释放地址 - 基地址) / 页大小
    uint32_t page_index = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, page_index, page_count, 0);

    mutex_unlock(&alloc->mutex);
}

static void show_mem_info(boot_info_t * boot_info)
{
    log_printf("mem region:");
    // 将boot程序传过来的boot_info内存信息打印出来
    for (int i = 0; i < boot_info->ram_region_count; i++)
    {
        log_printf("[%d]: 0x%x - 0x%x", i,
                    boot_info->ram_region_cfg[i].start,
                    boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

/**
 * @brief 获取可用的物理内存大小
 */
static uint32_t total_mem_size(boot_info_t * boot_info)
{
    int mem_size = 0;

    // 简单起见，暂不考虑中间有空洞的情况
    for (int i = 0; i < boot_info->ram_region_count; i++)
    {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}

/**
 * @brief 分配一个空闲的二级页表项, 并根据虚拟地址建立一级页目录项到二级页表项的映射关系
 * pde_t * page_dir  当前进程页目录地址
 * uint32_t vaddr    虚拟地址
 * int alloc         是否要分配页表项, 1-分配, 0-不分配
 */
static pte_t * find_pte(pde_t * page_dir, uint32_t vaddr, int alloc)
{
    pte_t * pte_table;

    // 根据虚拟地址的前10位在页目录中找到对应的项pde
    pde_t * pde = page_dir + pde_index(vaddr);
    if (pde->present) // 该页目录项已存在, 直接获取对应的页表项地址
    {
        pte_table = (pte_t *)pde_paddr(pde);
    }
    else
    {
        // 该页目录项不存在, 则考虑分配一个
        if (alloc == 0) // 不让分配, 直接返回NULL
        {
            return (pte_t *)0;
        }

        // 分配一个4KB物理页表给 新建的页表(可存放1024个页表项)
        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);
        if (pg_paddr == 0) // 分配失败, 直接返回NULL
        {
            return (pte_t *)0;
        }

        // 分配好新的页表, 需建立页目录项与页表的对应关系, 在设置好页表属性后(页表地址|存在|可读性|用户可访问)赋值给页目录项pde
        pde->v = pg_paddr | PDE_P | PDE_W | PDE_U;

        // 为物理页表绑定虚拟地址的映射, 这样下面就可以计算出虚拟地址了
        //kernel_pg_last[pde_index(vaddr)].v = pg_paddr | PDE_P | PDE_W;

        /**
         * 这里虚拟地址和物理地址一一映射, 所以直接写入
         * 清空页表中所有页表项, 防止以后使用时出现异常
         */
        pte_table = (pte_t *)(pg_paddr);
        kernel_memset(pte_table, 0, MEM_PAGE_SIZE);
    }

    // 根据虚拟地址中间10位在新建的页表中找到对应的页表项pte并返回
    return pte_table + pte_index(vaddr);
}

/**
 * @brief 将指定的地址空间进行一页的映射
 * pde_t * page_dir  页目录表数组首地址
 * uint32_t vaddr    待映射的虚拟内存起始地址
 * uint32_t paddr    物理内存起始地址
 * int count         映射的物理页数
 * uint32_t perm     页的权限
 */
static int memory_create_map(pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm)
{
    // 逐个物理页建立映射关系
    for (int i = 0; i < count; i++)
    {
        log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);
        // 在二级页表项中找一个空闲pte来建立与物理页的映射关系, 并将找到的页表项地址存放到一级页目录项中
        pte_t * pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0)
        {
            log_printf("create pte failed. pte == 0");
            return -1;
        }

        log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        // 创建映射的时候, 这条pte应当是不存在的, 如果存在, 说明找到的页表有问题
        ASSERT(pte->present == 0);

        // 设置映射的页表项的值: 物理地址 | 权限 | 存在位
        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief 根据内存映射表, 构造内核页表
 * 以下是内核的内存映射表项描述符
 * [0] P(Present), 1-内存映射表项存在, 0-不存在
 * [1] R/W(Read/Write), 0-只读, 1-可读写/可读执行
 * [2] U/S(User/Supervisor), 0-不允许用户访问, 1-同时允许用户和内核访问, 控制页表的访问权限
 * [3] PWT(Page level Write Through)
 * [4] PCD(Page level Cache Disable)
 * [5] A(Accessed)
 * [6] D(Dirty)
 * [7] PAT(Page Size), CR4 PSE=1时必须为0(4MB物理页), 否则为4KB物理页
 * [8] G(Global), CR4 PGE=1
 * [9:11] Ignored
 * [12:31] 4KB对齐的物理页映射地址
 */
static void create_kernel_table(void)
{
    // 以下地址参数均在 kernel.lds 链接脚本中定义
    extern uint8_t kernel_base[], s_text[], e_text[], s_data[];

    /**
     * 虚拟地址->物理页地址的映射表, 用于建立内核级的地址映射
     * 地址不变, 但是添加了属性/权限
     */
    static memory_map_t kernel_map[] =
    {
        //virtual start   virtual end                  physical start   permission
        // 内核栈区域(0～64KB内存区域), 此处也从0开始
        {kernel_base,     s_text,                      0,               PTE_W},
        // 内核代码段区域(代码段+只读数据段), 64KB开始, 此处虚拟地址和物理地址一样
        {s_text,          e_text,                      s_text,          PTE_R},
        // 内核数据区(全局数据段+bss数据段), 此处虚拟地址和物理地址一样
        {s_data,          (void *)(MEM_EBDA_START-1),  s_data,          PTE_W},
        {(void *)CONSOLE_DISP_ADDR, (void *)(CONSOLE_DISP_END - 1), (void *)CONSOLE_VIDEO_BASE, PTE_W},
        // 扩展存储空间(1MB ~ 128MB)一一映射, 方便直接操作
        {(void *)MEM_EXT_START,     (void *)MEM_EXT_END,            (void *)MEM_EXT_START,      PTE_W},
    };

    // 清空页目录表, 页目录表是静态分配的, 而后面的页表是动态分配的
    kernel_memset(kernel_page_dir, 0, sizeof(kernel_page_dir));

    // 清空后，然后依次根据映射关系创建映射表
    for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++)
    {
        memory_map_t * map = kernel_map + i;

        /**
         * 可能有多个页，建立多个页的配置
         * 简化起见，不考虑4MB的情况
         */
        uint32_t vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        uint32_t vend   = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        uint32_t paddr  = down2((uint32_t)map->pstart, MEM_PAGE_SIZE);
        int page_count  = (vend - vstart) / MEM_PAGE_SIZE;

        // 页目录表数组首地址, 待映射的虚拟内存起始地址, 物理内存起始地址, 页数, 权限
        memory_create_map(kernel_page_dir, vstart, (uint32_t)paddr, page_count, map->perm);
    }
}

/**
 * @brief 创建进程的初始页表 uvm - User Virturl Memory
 * 主要的工作创建页目录表，然后从内核页表中复制一部分
 * 虚拟内存              物理内存
 *
 * 0xFFFFFFFF(4GB)      0x08000000(128MB)
 *
 *                      进程1占用的物理内存页
 * 进程
 *                      进程2占用的物理内存页
 *
 * 0x80000000(2GB)      0x00100000(1MB)
 *
 * 操作系统              操作系统
 *
 * 0x00000000           0x00000000
 */
uint32_t memory_create_uvm(void)
{
    // 分配一页物理内存, 用于存放页表(一页内存4KB可保存1024个页表项), 并将分配的物理首地址赋给 page_dir 页目录项
    pde_t *   = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0)
    {
        return 0;
    }
    // 清空刚分配的1024页表项中内容
    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    /**
     * 复制整个内核空间的页目录项, 以便与其它进程共享内核空间
     * 用户空间的内存映射暂不处理, 等加载程序时创建
     * 设置从0 ~ 0x80000000(虚拟地址高10位)的进程内核态的页表项, 各进程共享的内核页表项(包括代码段, 数据段)
     */
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    for (int i = 0; i < user_pde_start; i++)
    {
        // 复制内核态的页表项
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}

/**
 * @brief 销毁用户空间内存
 */
void memory_destroy_uvm(uint32_t page_dir)
{
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    ASSERT(page_dir != 0);

    // 释放页表中对应的各项，不包含映射的内核页面
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++)
    {
        if (!pde->present)
        {
            continue;
        }

        // 释放页表对应的物理页 + 页表
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++)
        {
            if (!pte->present)
            {
                continue;
            }

            addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        }

        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);
    }

    // 释放页目录表
    addr_free_page(&paddr_alloc, page_dir, 1);
}

/**
 * @brief 复制页表及其所有的内存空间
 */
uint32_t memory_copy_uvm(uint32_t page_dir)
{
    // 创建一级页目录项, 复制内核态共享的页表项
    uint32_t to_page_dir = memory_create_uvm();
    if (to_page_dir == 0)
    {
        goto copy_uvm_failed;
    }

    // 再复制用户空间的各项
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    // 获得父进程一级页目录项
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    // 遍历用户空间页目录项
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++)
    {
        // 一级页目录项中present等于0, 说明指向的二级页表不存在, 直接continue
        if (!pde->present)
        {
            continue;
        }

        // 遍历找到的页目录项指定的页表
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++)
        {
            // 二级页表项中present等于0, 说明指向的物理页表不存在, 直接continue
            if (!pte->present)
            {
                continue;
            }

            // 若页表项指向的物理内存页存在, 则也分配一页物理内存给子进程
            uint32_t page = addr_alloc_page(&paddr_alloc, 1);
            if (page == 0)
            {
                goto copy_uvm_failed;
            }

            // 建立虚拟地址与物理地址的映射关系
            uint32_t vaddr = (i << 22) | (j << 12); // 页目录项 | 页表项
            // 页目录地址, 虚拟地址, 页地址, 页数, 权限
            int err = memory_create_map((pde_t *)to_page_dir, vaddr, page, 1, get_pte_perm(pte));
            if (err < 0)
            {
                goto copy_uvm_failed;
            }

            // 复制父进程物理内存页中内容, 待优化, 创建子进程不一定非要分配和拷贝物理页内存
            kernel_memcpy((void *)page, (void *)vaddr, MEM_PAGE_SIZE);
        }
    }
    return to_page_dir;

copy_uvm_failed:
    if (to_page_dir)
    {
        memory_destroy_uvm(to_page_dir);
    }
    return -1;
}

/**
 * @brief 获取指定虚拟地址的物理地址
 * 如果转换失败，返回0。
 */
uint32_t memory_get_paddr(uint32_t page_dir, uint32_t vaddr)
{
    pte_t * pte = find_pte((pde_t *)page_dir, vaddr, 0);
    if (pte == (pte_t *)0)
    {
        return 0;
    }

    return pte_paddr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}

/**
 * @brief 在不同的进程空间中拷贝字符串
 * page_dir为目标页表，当前仍为老页表
 */
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size)
{
    char *buf, *pa0;

    while(size > 0)
    {
        // 获取目标的物理地址, 也即其另一个虚拟地址
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if (to_paddr == 0)
        {
            return -1;
        }

        // 计算当前可拷贝的大小
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE - 1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        if (curr_size > size)
        {
            curr_size = size;       // 如果比较大，超过页边界，则只拷贝此页内的
        }

        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);

        size -= curr_size;
        to += curr_size;
        from += curr_size;
  }

  return 0;
}

uint32_t memory_alloc_for_page_dir(uint32_t page_dir, uint32_t vaddr, uint32_t size, int perm)
{
    uint32_t curr_vaddr = vaddr;
    int page_count = up2(size, MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    vaddr = down2(vaddr, MEM_PAGE_SIZE);

    // 逐页分配内存，然后建立映射关系
    for (int i = 0; i < page_count; i++)
    {
        // 分配需要的内存
        uint32_t paddr = addr_alloc_page(&paddr_alloc, 1);
        if (paddr == 0)
        {
            log_printf("mem alloc failed. no memory");
            return 0;
        }

        // 建立分配的内存与指定地址的关联
        int err = memory_create_map((pde_t *)page_dir, curr_vaddr, paddr, 1, perm);
        if (err < 0)
        {
            log_printf("create memory map failed. err = %d", err);
            addr_free_page(&paddr_alloc, vaddr, i + 1);
            return -1;
        }

        curr_vaddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief 为指定的虚拟地址空间分配多页内存, 主要是提供给应用进程调用
 */
int memory_alloc_page_for(uint32_t addr, uint32_t size, int perm)
{
    return memory_alloc_for_page_dir(task_current()->tss.cr3, addr, size, perm);
}


/**
 * @brief 分配一页内存
 * 主要用于内核空间内存的分配，不用于进程内存空间
 */
uint32_t memory_alloc_page(void)
{
    // 内核空间虚拟地址与物理地址相同
    return addr_alloc_page(&paddr_alloc, 1);
}

/**
 * @brief 释放一页内存
 */
void memory_free_page(uint32_t addr)
{
    if (addr < MEMORY_TASK_BASE)
    {
        // 内核空间, 直接释放
        addr_free_page(&paddr_alloc, addr, 1);
    }
    else
    {
        // 进程空间, 还要释放页表, 即分页机制中的页映射关系
        pte_t * pte = find_pte(current_page_dir(), addr, 0);
        ASSERT((pte != (pte_t *)0) && pte->present);

        // 释放内存页
        addr_free_page(&paddr_alloc, pte_paddr(pte), 1);

        // 释放页表
        pte->v = 0;
    }
}

/**
 * @brief 初始化内存管理子系统
 * 该函数的主要任务：
 * 1. 初始化物理内存分配器: 将所有物理内存管理起来, 在1MB内存中分配物理位图
 * 2. 重新创建内核页表: 原loader中创建的页表已经不再合适
 */
void memory_init(boot_info_t * boot_info)
{
    // 位于1MB空间内, 内核代码段,数据段之后的内存块, 在链接脚本中定义
    extern uint8_t * mem_free_start;

    log_printf("start memory init...");
    show_mem_info(boot_info);

    // 在内核数据后面放物理页位图, 该地址由内核kernel.lds文件确定128KB区域物理页位图的起始地址
    /* first_task 初始进程的配置: 紧接着在低端1MB内, 内核数据段后的e_data开始存储, 但是运行时搬运到虚拟地址 0x80000000 处 */
    uint8_t * mem_bitmap_addr = (uint8_t *)&mem_free_start;

    // 计算1MB以上物理内存空间的空闲内存总容量，并对齐的页边界
    uint32_t mem_up1MB_free_size = total_mem_size(boot_info) - MEM_EXT_START;
    mem_up1MB_free_size = down2(mem_up1MB_free_size, MEM_PAGE_SIZE);   // 对齐到4KB页, 向下对齐, 保证物理内存页数正确
    // 物理内存开始位置=MEM_EXT_START(1MB), 4KB对齐后的总物理内存大小为 mem_up1MB_free_size
    log_printf("Free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free_size);

    // 4GB大小需要总共4*1024*1024*1024/4096/8=128KB的位图, 因此使用低1MB的RAM空间中足够
    // 该部分的内存紧跟在mem_free_start开始的128KB区域, 在kernel.lds文件中定义位图数组的起始地址 mem_bitmap_addr
    addr_alloc_init(&paddr_alloc, mem_bitmap_addr, MEM_EXT_START, mem_up1MB_free_size, MEM_PAGE_SIZE);
    mem_bitmap_addr += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);

    // 到这里, mem_bitmap_addr 必须比EBDA起始地址要小, 保证128KB bitmap区域不会被覆盖
    ASSERT(mem_bitmap_addr < (uint8_t *)MEM_EBDA_START);

    // 重新创建内核的内存映射页表
    create_kernel_table();

    // 将页目录表项基地址写入CR3寄存器
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}

/**
 * @brief 通过调整进程的动态内存分配区的边界来分配或释放内存
 *        当进程需要动态分配内存时, 系统调用brk()会将动态分配区的底部边界heap_end往上推, 从而增加可用的内存空间
 *        而当需要释放内存时, 可以通过munmap()解除部分区间的映射来回收内存
 * int incr  申请的动态内存大小
 * char *    返回堆之前的指针
 */
char * sys_sbrk(int incr)
{
    task_t * task = task_current();
    char * pre_heap_end = (char * )task->heap_end;
    int pre_incr = incr;

    ASSERT(incr >= 0);

    // 如果申请的大小为0, 则返回有效的heap区域的顶端
    if (incr == 0)
    {
        log_printf("sbrk(0): end = 0x%x", pre_heap_end);
        return pre_heap_end;
    }

    uint32_t start = task->heap_end;
    uint32_t end = start + incr;

    // 起始偏移非0, 不是4KB页对齐
    int start_offset = start % MEM_PAGE_SIZE;
    if (start_offset)
    {
        // 若申请的内存大小+start不超过1页, 只调整一下end位置即可
        if (start_offset + incr <= MEM_PAGE_SIZE)
        {
            task->heap_end = end;
            return pre_heap_end; // 返回申请的内存起始地址
        }
        // 若申请的内存大小+start超过1页, 先只调本页的
        else
        {
            uint32_t curr_size = MEM_PAGE_SIZE - start_offset;
            start += curr_size;
            incr -= curr_size;
        }
    }

    // 处理其余的，起始对齐的页边界的
    if (incr)
    {
        uint32_t curr_size = end - start;
        int err = memory_alloc_page_for(start, curr_size, PTE_P | PTE_U | PTE_W);
        if (err < 0)
        {
            log_printf("sbrk: alloc mem failed.");
            return (char *)-1;
        }
    }

    //log_printf("sbrk(%d): end = 0x%x", pre_incr, end);
    task->heap_end = end;
    return (char * )pre_heap_end;
}



#if 0
/**
 * MA - Memory Area, 系统将物理内存分三个区域: 硬件区、内核区、应用区
 * 硬件区: 0x00000000 ~ 0x1FFFFFFF  32MB, DMA, 硬件寄存器
 * 内核区: 0x20000000 ~ 0x3FFFFFFF  32MB
 * 应用区: 0x40000000 ~ 0xFFFFFFF
 */
#define MA_TYPE_HW         1  // 内存硬件区
#define MA_TYPE_KERNEL     2  // 内存内核区
#define MA_TYPE_APP        3  // 内存应用区

// 从逻辑上划分物理内存三大区域
#define MA_HW_START        (0)
#define MA_HW_SIZE         (0x2000000)
#define MA_HW_END          (MA_HW_START + MA_HW_SIZE - 1)
#define MA_KERNEL_START    (0x2000000)
#define MA_KERNEL_SIZE     (0x40000000 - 0x2000000)
#define MA_KERNEL_END      (MA_KERNEL_START + MA_KERNEL_SIZE - 1)
#define MA_APP_START       (0x40000000)
#define MA_APP_SIZE        (0xffffffff - 0x40000000)
#define MA_APP_END         (MA_APP_START + MA_APP_SIZE)


/**
 * 描述一个内存区域的数据结构
 */
typedef struct _mem_area_t
{
    //spinlock_t ma_lock;           // 保护内存区的自旋锁
    sem_t ma_sem;                 // 内存区的信号量
    list_t ma_list;               // 内存区自身的链表
    uint8_t ma_status;            // 内存区的状态
    uint8_t ma_flgs;              // 内存区的标志 
    uint8_t ma_type;              // 内存区的类型
    list_t  ma_wait_list;         // 内存区的等待队列
    uint32_t ma_max_pages;        // 内存区总的页面数
    uint32_t ma_alloc_pages;      // 内存区已分配的页面数
    uint32_t ma_free_pages;       // 内存区空闲的页面数
    uint32_t ma_reserve_pages;    // 内存区保留的页面数
    uint32_t ma_horizline;        // 内存区分配时的水位线
    uint32_t ma_start;            // 内存区开始地址
    uint32_t ma_end;              // 内存区结束地址
    uint32_t ma_size;             // 内存区大小
} mem_area_t;


/**
 * 描述指定order页面属性的结构体
 */
typedef struct _bafhlist_t
{
    spinlock_t af_lock;           // 保护自身结构的自旋锁
    uint8_t af_stus;              // 状态 
    uint32_t af_order;            // 页面数的位移量
    uint32_t af_order_nr;         // order对应的页面数, 如: order为2, 即1<<2=4
    uint32_t af_free_nr;          // 多少个空闲msadsc_t结构, 即空闲页面
    uint32_t af_mobjnr;           // 此结构的msadsc_t结构总数, 即此结构总页面
    uint32_t af_alloc_index;      // 此结构的分配计数
    uint32_t af_free_index;       // 此结构的释放计数
    list_t af_free_list;          // 挂载此结构的空闲msadsc_t结构
    list_t af_alloc_list;         // 挂载此结构已经分配的msadsc_t结构
} bafhlist_t;


/**
 * 管理所有order页面
 * mdm_list 数组中第 0 个元素挂载1个 msadsc_t 结构
 *   它们的物理内存地址可能对应于 0x1000、0x3000、0x5000
 * mdm_list 数组中第 1 个元素挂载2个连续的 msadsc_t 结构
 *   它们的物理内存地址可能对应于 0x8000～0x9FFF、0xA000～0xBFFF
 * mdm_list 数组中第 2 个元素挂载4个连续的 msadsc_t 结构
 *   它们的物理内存地址可能对应于 0x100000～0x103FFF、0x104000～0x107FFF...
 * mdm_list 数组挂载连续 msadsc_t 结构的数量等于用 1 左移其数组下标
 *   如数组下标为 3,那结果就是 8（1<<3）个连续的 msadsc_t 结构
 * 我们并不在意其中第一个 msadsc_t 结构对应的内存物理地址从哪里开始
 *   但是第一个 msadsc_t 结构与最后一个 msadsc_t 结构, 它们之间的内存物理地址是连续的
 */
#define MEM_DIV_MAX         (12)
typedef struct _mem_div_mgr_t
{
    spinlock_t mdm_lock;        // 保护自身结构的自旋锁
    uint32_t mdm_stus;          // 状态
    uint32_t mdm_divnr;         // 内存分配次数
    uint32_t mdm_mernr;         // 内存合并次数
    bafhlst_t mdm_list[MEM_DIV_MAX];  //bafhlst_t结构数组
    bafhlst_t mdm_one_list;     // 单个的bafhlst_t结构
} mem_div_mgr_t;
#endif

