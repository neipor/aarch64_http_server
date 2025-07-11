# ANX HTTP Server - 系统调用参考手册

**作者**: neipor  
**邮箱**: [neitherportal@proton.me](mailto:neitherportal@proton.me)

## 1. 概述

本文档记录了ANX HTTP Server在Linux (AArch64)平台上运行时，所依赖的关键系统调用。虽然ANX主要使用标准C库函数（如 `printf`, `malloc`, `socket`），但这些库函数最终会调用底层的Linux系统调用来与内核交互。理解这些底层调用有助于性能分析和深度调试。

---

## 2. AArch64 系统调用约定

-   **系统调用号**: 存放在 `x8` 寄存器。
-   **参数传递**: 前6个参数依次放入 `x0` 至 `x5` 寄存器。
-   **触发**: 使用 `svc #0` (Supervisor Call) 指令陷入内核态。
-   **返回值**: 存放在 `x0` 寄存器。负值通常表示错误，其绝对值为`errno`。

---

## 3. 关键系统调用列表

下表列出了ANX核心功能所依赖的主要系统调用。

| C库函数 / 功能 | 系统调用 | 调用号 (AArch64) | 主要用途 |
| :--- | :--- | :--- | :--- |
| **网络** | | | |
| `socket()` | `socket` | 198 | 创建一个新的Socket。 |
| `bind()` | `bind` | 200 | 将Socket绑定到指定IP和端口。 |
| `listen()` | `listen` | 201 | 开始监听进入的连接。 |
| `accept()` / `accept4()` | `accept4` | 242 | 接受一个新的客户端连接。 |
| `send()` / `write()` | `write` | 64 | 发送数据到Socket。 |
| `recv()` / `read()` | `read` | 63 | 从Socket接收数据。 |
| `close()` | `close` | 57 | 关闭一个文件描述符。 |
| **事件驱动** | | | |
| `epoll_create()` | `epoll_create1` | 20 | 创建一个epoll实例。 |
| `epoll_ctl()` | `epoll_ctl` | 21 | 添加/修改/删除epoll监听的描述符。|
| `epoll_wait()` | `epoll_pwait` | 213 | 等待网络事件发生。 |
| **文件I/O** | | | |
| `fopen()` / `open()` | `openat` | 56 | 打开一个文件。 |
| `fread()` / `read()` | `read` | 63 | 从文件读取数据。 |
| `fwrite()` / `write()`| `write`| 64 | 写入数据到文件。 |
| `sendfile()` | `sendfile` | 71 | 在两个文件描述符之间零拷贝数据。|
| `stat()` | `newfstatat` | 79 | 获取文件元数据（如大小、修改时间）。 |
| **进程管理** | | | |
| `fork()` | `clone` | 220 | 创建一个新的Worker进程。 |
| `exit()` / `_exit()`| `exit_group` | 94 | 退出进程。 |
| `kill()` | `kill` | 129 | 向进程发送信号。 |
| `waitpid()` | `wait4` | 260 | 等待子进程状态改变。 |
| **内存管理** | | | |
| `malloc()` / `free()`| `brk`, `mmap`, `munmap`| 12, 222, 215 | 分配和释放堆内存。 |

---

> 此列表并非详尽无遗，但涵盖了构成ANX服务器核心功能的最重要的系统调用。 