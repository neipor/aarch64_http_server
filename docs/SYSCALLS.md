# ANX 系统调用参考 (AArch64)

本文档记录了 ANX 项目中使用的 Linux 系统调用及其在 AArch64 架构下的约定。

## 调用约定

- 系统调用号放入 `x8` 寄存器。
- 参数依次放入 `x0`, `x1`, `x2`, `x3`, `x4`, `x5` 寄存器。
- 使用 `svc #0` 指令触发系统调用。
- 返回值通常在 `x0` 寄存器中。

## 常用系统调用

| 功能 | 系统调用号 | `x0` | `x1` | `x2` | `x3` | `x4` | `x5` |
|---|---|---|---|---|---|---|---|
| `read` | 63 | `unsigned int fd` | `char *buf` | `size_t count` | | | |
| `write` | 64 | `unsigned int fd` | `const char *buf`| `size_t count`| | | |
| `close` | 57 | `unsigned int fd` | | | | | |
| `exit` | 93 | `int error_code` | | | | | |
| `socket` | 198 | `int domain` | `int type` | `int protocol` | | | |
| `bind` | 200 | `int fd` | `struct sockaddr *umyaddr` | `int addrlen` | | | |
| `listen` | 201 | `int fd` | `int backlog` | | | | |
| `accept`| 202 | `int fd` | `struct sockaddr *upeer_sockaddr`| `int *upeer_addrlen`| | | |
| `epoll_create1`| 291 | `int flags` | | | | | |
| `epoll_ctl` | 233 | `int epfd` | `int op` | `int fd` | `struct epoll_event *event` | | |
| `epoll_wait` | 232 | `int epfd` | `struct epoll_event *events` | `int maxevents` | `int timeout` | | |

*注：此列表会随着项目进展而更新。* 