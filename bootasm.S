#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# 启动第一个 CPU：从实模式切换到 32 位保护模式，并跳转到 C 代码
# BIOS 会将硬盘第 0 扇区（512 字节）加载到物理地址 0x7c00
# 并在实模式下开始执行（此时 CS=0，IP=0x7c00）

.code16                       # 指示汇编器生成 16 位实模式代码
.globl start
start:
  cli                         # BIOS 可能开启了中断，启动阶段先关闭中断

  # 将数据段寄存器 DS、ES、SS 清零
  # 此时还在实模式下，确保段寄存器处于已知状态
  xorw    %ax,%ax             # AX = 0
  movw    %ax,%ds             # 数据段 DS = 0
  movw    %ax,%es             # 附加段 ES = 0
  movw    %ax,%ss             # 栈段 SS = 0

  # 启用 A20 地址线
  # 早期 PC 为了兼容性，将地址线 A20 关闭，导致地址回绕在 1MB 内
  # 若不打开 A20，将无法正确访问 1MB 以上内存
seta20.1:
  inb     $0x64,%al           # 读取键盘控制器状态端口
  testb   $0x2,%al            # 检查输入缓冲区是否为空
  jnz     seta20.1            # 若忙则等待

  movb    $0xd1,%al           # 发送命令：准备写输出端口
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al           # 再次等待键盘控制器空闲
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al           # 设置 A20 使能位
  outb    %al,$0x60

  # 从实模式切换到 32 位保护模式
  # 使用一个临时 GDT，使虚拟地址与物理地址保持一一映射
  lgdt    gdtdesc             # 加载全局描述符表（GDT）
  movl    %cr0, %eax          # 读取控制寄存器 CR0
  orl     $CR0_PE, %eax       # 设置保护模式使能位（PE）
  movl    %eax, %cr0          # 写回 CR0，正式开启保护模式

//PAGEBREAK!
  # 通过一次远跳转（ljmp）完成模式切换
  # 该跳转会刷新 CS 寄存器，并进入 32 位代码段
  # 此时仍然是“平坦映射”，虚拟地址等于物理地址
  ljmp    $(SEG_KCODE<<3), $start32

.code32                      # 指示汇编器从这里开始生成 32 位代码
start32:
  # 设置保护模式下的数据段寄存器
  movw    $(SEG_KDATA<<3), %ax    # 数据段选择子
  movw    %ax, %ds                # DS：数据段
  movw    %ax, %es                # ES：附加段
  movw    %ax, %ss                # SS：栈段
  movw    $0, %ax                 # FS、GS 尚未使用，清零
  movw    %ax, %fs
  movw    %ax, %gs

  # 设置栈指针，并跳转到 C 语言实现的 bootmain
  # 此时已经处于 32 位保护模式
  movl    $start, %esp            # 设置临时栈顶
  call    bootmain                # 进入 bootloader 的 C 代码

  # 如果 bootmain 返回（理论上不应该发生）
  # 向 Bochs 模拟器发送断点信号，然后死循环
  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx
spin:
  jmp     spin

# -------------------------------
# 全局描述符表（GDT）
# -------------------------------
.p2align 2                          # 强制 4 字节对齐
gdt:
  SEG_NULLASM                       # 空描述符（必须）
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # 内核代码段（可执行、可读）
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # 内核数据段（可写）

gdtdesc:
  .word   (gdtdesc - gdt - 1)       # GDT 大小（字节数 - 1）
  .long   gdt                       # GDT 在内存中的地址