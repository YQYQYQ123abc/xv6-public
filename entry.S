# xv6 内核启动入口
# 这个文件链接到内核 C 代码，可以引用内核符号，例如 main()
# bootasm.S + bootmain.c 会跳转到这里

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# ----------------------------
# Multiboot 头，用于 Multiboot 引导程序（如 GRUB）
# ----------------------------
.p2align 2
.text
.globl multiboot_header
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# ----------------------------
# ELF 默认入口符号 _start
# ----------------------------
.globl _start
_start = V2P_WO(entry)   # ELF 物理入口地址

# ----------------------------
# 内核真正入口，分页尚未开启
# ----------------------------
.globl entry
entry:
  # ----------------------------
  # 1. 打开 4MB 页扩展
  # ----------------------------
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4

  # ----------------------------
  # 2. 设置页目录
  # ----------------------------
  movl    $(V2P_WO(entrypgdir)), %eax
  movl    %eax, %cr3

  # ----------------------------
  # 3. 开启分页
  # ----------------------------
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # ----------------------------
  # 4. 设置内核栈
  # ----------------------------
  movl $(stack + KSTACKSIZE), %esp

  # ----------------------------
  # 5. 打印内核入口信息
  #    使用 cprintf 打印 “[KERNEL] entry reached”
  # ----------------------------
  # 注意：这里不能直接调用 C 函数 cprintf（因为栈还没完全初始化完），
  # 如果希望打印，可以在 main() 一进入就打印
  # 这里用注释标明入口
  # [KERNEL] entry reached

  # ----------------------------
  # 6. 跳转到 main() 函数
  # ----------------------------
  mov $main, %eax
  jmp *%eax

# ----------------------------
# 内核栈
# ----------------------------
.comm stack, KSTACKSIZE