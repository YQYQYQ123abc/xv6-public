// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  // 临时缓冲区，用于存放 ELF 头
  elf = (struct elfhdr*)0x10000;

  // ----------------------------
  // 1. 进入 bootmain
  // ----------------------------
  cprintf("[BOOT] enter bootmain\n"); // 输出调试信息，标记进入 bootmain 阶段

  // 从磁盘读取内核的第 1 页（4 KB）
  readseg((uchar*)elf, 4096, 0);

  // ----------------------------
  // 2. ELF 文件合法性检查
  // ----------------------------
  if(elf->magic != ELF_MAGIC)
    return;  // ELF 魔数不对，交由 bootasm.S 处理错误

  cprintf("[BOOT] elf header loaded\n"); // ELF 文件头读取完成

  // 逐段加载内核（忽略段的标志）
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;         // 获取物理加载地址
    readseg(pa, ph->filesz, ph->off); // 从磁盘读取该段内容到物理地址
    if(ph->memsz > ph->filesz)      // 如果内存大小大于文件大小，用 0 填充剩余空间
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // ----------------------------
  // 3. 内核段加载完成
  // ----------------------------
  cprintf("[BOOT] kernel loaded\n"); // 所有内核段已加载完毕

  // 调用 ELF 入口地址，跳转进入内核
  entry = (void(*)(void))(elf->entry);
  entry(); // 不会返回
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, dst, SECTSIZE/4);
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1;

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
