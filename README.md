# Virtual-Terminal 软件包

## 简介

RT_Vterm 是基于 RT-Thread 内核与 RT-Tunnel 组件的**虚拟终端组件**，核心功能是实现嵌入式系统的 I/O 重定向、双向数据传输与命令交互。该组件通过 RT-Tunnel 的 “上下行隧道”（up_tunnel/down_tunnel）实现数据可靠传输，兼容 RT-Thread 字符设备模型，支持将控制台 / FinSH 输出切换到虚拟终端，同时提供命令注册、输入辅助等功能，适用于远程调试、串口复用、Web 终端扩展等场景。



## 主要特性

- **基于 RT-Tunnel 双向传输**：通过 `up_tunnel`（上行，设备→主机，ID：0x56545455）和 `down_tunnel`（下行，主机→设备，ID：0x56545444）实现数据隔离与可靠传输，支持线程安全读写。
- **RT-Thread 设备模型兼容**：注册为 “vterm” 字符设备，支持 `open/read/write` 标准接口，可直接作为控制台或 FinSH 的 I/O 设备。
- **灵活 I/O 切换与恢复**：提供 `vterm_console` 命令将控制台 / FinSH 切换到虚拟终端，`restore_original` 命令恢复原始 I/O 设备（如物理串口），切换过程无数据丢失。
- **命令注册与输入辅助**：支持自定义命令注册（最大 8 条，`vterm_cmd_register` 接口），提供 `vterm_input_assist_thread` 线程处理输入回显、换行转换（\n→\r\n），适配终端显示习惯。
- 多模式数据读写：
  - 阻塞读：`RT_Vterm_WaitKey` 阻塞等待并读取 1 字节；
  - 非阻塞读：`RT_Vterm_GetKey` 无数据时返回 -1；
  - 单字符写：`RT_Vterm_PutChar` 简化单个字符传输；
  - 缓冲区状态查询：`RT_Vterm_HasData` 检查下行数据是否就绪。
- **自动换行转换**：写入数据时自动将 `\n` 转为 `\r\n`，确保终端显示换行正常，无需手动处理格式。



## 快速上手

### 初始化流程

RT_Vterm 需在 RT-Tunnel 初始化后启动，推荐初始化顺序：

1. **初始化 RT-Tunnel**：确保 `RT_Tunnel_Init` 先执行（通常自动初始化）；
2. **初始化 RT_Vterm**：调用 `RT_Vterm_Init` 分配上下行隧道并配置操作模式；
3. **shell切换到虚拟终端设备**：调用 `rt_hw_vterm_console_init` 注册 “vterm” 字符设备，并对接shell；



## 注意

确保 RT-Tunnel 已启用，且 `TUNNEL_NUM` 至少为 2
