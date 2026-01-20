#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

// 启动其他 CPU 的函数
static void startothers(void);

// 当前 CPU 初始化完成后执行的函数
static void mpmain(void)  __attribute__((noreturn));

extern pde_t *kpgdir;
extern char end[]; // 内核 ELF 文件加载后的首地址

// -----------------------------------
// 内核引导处理器 C 代码入口
// -----------------------------------
int
main(void)
{
  // ----------------------------
  // 在 main() 一进入就打印，标记内核启动
  // ----------------------------
  cprintf("[KERNEL] main() started\n");

  // 初始化物理页分配器
  kinit1(end, P2V(4*1024*1024));

  // 分配内核页表
  kvmalloc();

  // 探测其他 CPU
  mpinit();

  // 初始化本地 APIC（中断控制器）
  lapicinit();

  // 初始化段描述符
  seginit();

  // 初始化可编程中断控制器
  picinit();

  // 初始化 I/O APIC（中断控制器）
  ioapicinit();

  // 初始化控制台硬件
  consoleinit();

  // 初始化串口
  uartinit();

  // 初始化进程表
  pinit();

  // 初始化陷阱向量
  tvinit();

  // 初始化缓冲区缓存
  binit();

  // 初始化文件表
  fileinit();

  // 初始化磁盘
  ideinit();

  // 启动其他 CPU
  startothers();

  // 初始化剩余物理页（必须在 startothers() 后执行）
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP));

  // 创建第一个用户进程
  userinit();

  // 当前 CPU 启动完成，进入多 CPU 主循环
  mpmain();
}

// -------------------------------------------------
// 其他 CPU 进入这里（由 entryother.S 跳转）
// -------------------------------------------------
static void
mpenter(void)
{
  switchkvm();   // 切换到内核页表
  seginit();     // 初始化段描述符
  lapicinit();   // 初始化本地 APIC
  mpmain();      // 进入多 CPU 主循环
}

// -------------------------------------------------
// 当前 CPU 公共初始化
// -------------------------------------------------
static void
mpmain(void)
{
  // 输出 CPU 启动信息
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());

  // 加载中断描述符表
  idtinit();

  // 通知 startothers() 这个 CPU 已经启动
  xchg(&(mycpu()->started), 1);

  // 启动调度器
  scheduler();
}

// -------------------------------------------------
// 启动非引导 CPU
// -------------------------------------------------
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // 将 entryother.S 放到未使用内存 0x7000
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // 引导 CPU 已启动
      continue;

    // 设置非引导 CPU 栈、入口函数、页表
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    // 启动 AP
    lapicstartap(c->apicid, V2P(code));

    // 等待 CPU 完成 mpmain()
    while(c->started == 0)
      ;
  }
}

// -------------------------------------------------
// 内核启动页表（entry.S 和 entryother.S 使用）
// -------------------------------------------------
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // 映射虚拟地址 [0, 4MB) 到物理地址 [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // 映射虚拟地址 [KERNBASE, KERNBASE+4MB) 到物理地址 [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
