# ARM 架构迁移修改清单（本项目）

> 目标：将当前以 x86/Bochs 为主的内核与启动链路迁移到 ARM 平台（以 ARMv7-A/ARMv8-A 为参考）。以下按模块列出需要修改与新增的内容，并给出本项目中可能涉及的文件位置与实现要点。

## 1. 构建与工具链
- **交叉编译器**：改用 `arm-none-eabi-*`（裸机）或 `aarch64-linux-gnu-*`（AArch64）工具链。
- **编译选项**：
  - 去除 `-m32`、`-mno-red-zone` 等 x86 选项；
  - 增加 `-mcpu`/`-march`、`-marm`/`-mthumb`（ARMv7）或 `-march=armv8-a`（AArch64）。
- **链接脚本**：新增/替换 ARM 链接脚本（入口、段地址、对齐、向量表位置）。
- **构建系统**：更新 Makefile 中目标架构、目标输出（ELF/IMG/DTB/UBoot image）。

## 2. 启动链路与引导
- **引导方式替换**：
  - x86 BIOS/GRUB 启动替换为 **U-Boot** 或 **固件/BootROM** 进入内核；
  - 若使用 QEMU/板卡，需匹配其加载地址与入口。
- **启动汇编**：
  - 替换 `src/boot/boot.asm`、`src/boot/loader.asm` 和 `src/kernel/start.asm` 的 x86 汇编；
  - 新增 ARM 启动汇编（初始化栈、关闭/配置 MMU、设置异常向量表）。
- **内核入口**：
  - x86 的 multiboot 解析替换为 ARM 传参约定（ATAG 或 Device Tree Blob）。

## 3. 中断/异常系统
- **中断控制器**：
  - 替换 x86 PIC/APIC 逻辑为 **GIC**（ARM Generic Interrupt Controller）；
  - 适配 `set_interrupt_handler`、`set_interrupt_mask` 等接口实现。
- **异常向量表**：
  - 新增 ARM 异常向量表与异常处理流程；
  - 适配 `interrupt.c`/`gate.c` 等模块。

## 4. 内存管理与地址空间
- **分页机制**：
  - x86 页表与 CR0/CR3 逻辑改为 ARM 的 TTBR、TCR、MAIR、SCTLR；
  - 页表格式改为 ARMv7（短描述符）或 ARMv8（4 级页表）。
- **内核映射与物理内存**：
  - 重新定义内核虚拟地址布局；
  - 内存初始化流程需读取 DTB 中的内存信息。

## 5. 端口 I/O 与 MMIO
- **I/O 指令替换**：
  - `inb/outb/inw/outw` 等端口 I/O（x86）替换为 MMIO 读写。
- **驱动适配**：
  - 所有硬件寄存器访问逻辑需基于 MMIO 地址，或由 DTB 解析。

## 6. 设备树（DTB）
- **设备发现**：
  - x86 固定端口/IRQ 改为设备树解析；
  - 已有 `src/kernel/devicetree.c` 可继续使用并扩展。
- **设备节点**：
  - 添加 UART、GIC、定时器、存储控制器（MMC/SD/IDE/SATA/virtio）节点。

## 7. 时钟与定时器
- **定时器来源**：
  - 替换 PIT/RTC（x86）为 ARM 通用定时器（Generic Timer）或 SoC 定时器；
  - 更新 `clock.c`/`time.c`。

## 8. 线程切换与上下文
- **上下文保存/恢复**：
  - 替换 `schedule.asm`、`handler.asm` 中的 x86 保存现场方式；
  - 实现 ARM 寄存器组保存、异常返回（`eret`/`subs pc, lr, #...`）。

## 9. 控制台与串口
- **UART 驱动**：
  - 使用 PL011/UART16550 等常见 UART；
  - 替换 `console.c` 的输出为串口或 framebuffer。

## 10. 存储与块设备
- **IDE/PATA**：
  - ARM 平台通常不使用 x86 IDE 端口（0x1F0/0x170）；
  - 建议改为 **MMC/SD** 或 **virtio-blk**，或使用 SoC 提供的 SATA/ATA 控制器。
- **本项目 IDE 驱动**：
  - `src/kernel/ide.c` 需要改为 MMIO + DTB 获取基址/IRQ；
  - 中断号来源 GIC；
  - 若使用 virtio，新增驱动并替换设备注册。

## 11. 启动参数与日志
- **内核参数**：
  - ARM 常通过 DTB 或 U-Boot 传递启动参数；
  - 替换 multiboot 相关结构。
- **日志输出**：
  - 依赖 UART 初始化顺序。

## 12. 运行与仿真环境
- **仿真器**：
  - x86 Bochs 改为 QEMU (`qemu-system-arm`/`qemu-system-aarch64`)；
  - 更新 `bochs` 相关脚本与配置。

## 13. 必要的新增文件/目录建议
- `src/arch/arm/`：ARM 相关的启动、异常、中断、MMU、上下文切换。
- `src/arch/arm/include/`：ARM 寄存器、GIC、UART、页表等定义。
- `src/linker/arm.ld`：ARM 链接脚本。
- `src/drivers/uart_pl011.c` / `src/drivers/gic.c` / `src/drivers/mmc.c` 或 `virtio_blk.c`。

## 14. 迁移步骤建议（最小可启动）
1. 使用 ARM 交叉编译器编译最小内核（只含 `printk` 输出）。
2. 增加启动汇编、建立栈、初始化 UART，串口输出 “hello”.
3. 加入异常向量表与中断控制器初始化。
4. 加入定时器中断与简单调度。
5. 接入块设备（MMC/virtio），再恢复文件系统或分区解析。

---

> 备注：当前项目已有 `devicetree` 解析基础，迁移到 ARM 时应当以 DTB 为硬件描述核心。IDE 驱动（基于端口 I/O）在 ARM 上通常不可复用，建议替换为 SoC 相关存储控制器或 virtio。
