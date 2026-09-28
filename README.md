# ChatRoomProject

一个使用 C++17 编写的 Linux TCP 聊天室项目。

服务端使用非阻塞 Socket 和 `epoll` 管理多个客户端，客户端使用 `poll` 同时监听终端输入与服务器消息。通信采用简单的、以换行符分隔的纯文本协议，因此既可以使用项目自带客户端，也可以使用 `nc` 连接和测试。

## 功能

- 支持多个客户端同时在线
- 用户名注册与重名检查
- 聊天室群聊
- 指定用户私聊
- 修改用户名
- 查看在线用户
- 用户加入和离开通知
- 服务端非阻塞收发与发送缓冲区
- 发送队列高低水位背压与慢客户端断开保护
- 通过 `signalfd` 处理 `SIGINT`/`SIGTERM` 并安全退出
- 带时间戳和级别过滤的线程安全日志
- 客户端同时处理终端输入和服务器消息
- 支持使用管道向客户端输入消息
- 支持使用 `nc` 作为测试客户端

## 环境要求

- Linux
- 支持 C++17 的编译器，例如 GCC 或 Clang
- CMake 3.22 或更高版本

可以使用以下命令检查本机环境：

```bash
g++ --version
cmake --version
```

本项目使用了 `epoll`、`accept4` 和 POSIX Socket 等 Linux API，目前不支持直接在 Windows 上编译运行。

## 编译

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build
```

编译完成后生成两个程序：

```text
build/bin/server
build/bin/client
```

如果希望并行编译，可以使用：

```bash
cmake --build build -j
```

## 运行

### 启动服务端

```bash
./build/bin/server
```

服务端默认监听：

```text
0.0.0.0:8080
```

`0.0.0.0` 表示监听本机所有 IPv4 网络接口。

按下 `Ctrl+C` 或向服务端发送 `SIGTERM` 时，信号会通过 `signalfd` 进入 `epoll` 事件循环。服务端停止接受新连接，关闭现有连接并统一释放资源后正常退出。

### 使用项目客户端连接

打开另一个终端：

```bash
./build/bin/client
```

当前客户端默认连接：

```text
127.0.0.1:8080
```

连接成功后，根据提示输入用户名即可进入聊天室。

### 使用 nc 连接

也可以使用 `nc` 作为客户端：

```bash
nc 127.0.0.1 8080
```

如果需要测试多个用户，可以打开多个终端并分别执行客户端或 `nc`。

### 使用管道输入

项目客户端支持从标准输入读取多行消息：

```bash
printf 'alice\nhello everyone\n/list\n/quit\n' | ./build/bin/client
```

每条消息必须以换行符结尾。管道测试建议以 `/quit` 结束，让客户端和服务端正常关闭连接。

## 聊天命令

| 命令 | 说明 |
| --- | --- |
| `/help` | 显示命令帮助 |
| `/list` | 查看当前在线用户 |
| `/rename <new_name>` | 修改用户名 |
| `/msg <user> <message>` | 向指定用户发送私聊消息 |
| `/quit` | 离开聊天室并断开连接 |

使用示例：

```text
alice
hello everyone
/msg bob hello
/rename alice2
/list
/quit
```

连接建立后的第一行输入会被当作用户名。成功加入后，不以 `/` 开头的消息会广播给聊天室中的所有用户。

## 通信协议

客户端与服务端使用 TCP 长连接和换行符分隔的纯文本协议：

```text
一行输入 = 一条完整消息
```

基本流程：

```text
建立 TCP 连接
    ↓
服务端提示输入用户名
    ↓
客户端发送用户名和换行符
    ↓
普通文本用于群聊，以 / 开头的文本作为命令
    ↓
/quit 或连接关闭时离开聊天室
```

TCP 本身没有消息边界，因此服务端和客户端都会维护接收缓冲区，并在遇到换行符时提取一条完整消息。一次 `recv()` 可能只收到半条消息，也可能同时收到多条消息。

## 项目结构

```text
.
├── include
│   ├── common
│   │   └── logger.h
│   ├── client
│   │   └── chat_client.h
│   └── server
│       ├── chat_room.h
│       ├── chat_service.h
│       ├── client_connection.h
│       └── epoll_server.h
├── src
│   ├── common
│   │   └── logger.cpp
│   ├── client
│   │   ├── chat_client.cpp
│   │   └── main.cpp
│   └── server
│       ├── chat_room.cpp
│       ├── chat_service.cpp
│       ├── client_connection.cpp
│       ├── epoll_server.cpp
│       └── main.cpp
├── tests
│   ├── integration
│   │   └── test_chat_server.cpp
│   ├── support
│   │   └── test_helpers.h
│   ├── unit
│   │   ├── test_chat_room.cpp
│   │   ├── test_chat_service.cpp
│   │   ├── test_client_connection.cpp
│   │   ├── test_logger.cpp
│   │   └── test_main.cpp
│   └── CMakeLists.txt
├── scripts
│   ├── load_test.py
│   └── slow_client_test.py
├── CMakeLists.txt
├── LICENSE
└── README.md
```

## 代码结构

### 服务端

- `EpollServer`：创建监听 Socket，通过 `epoll` 接收连接并处理客户端读写事件。
- `EpollServer` 同时监听 `signalfd`，负责处理 `SIGINT`、`SIGTERM` 和服务端停机清理。
- `ClientConnection`：保存单个客户端的接收缓冲区和发送缓冲区，处理半包与部分发送。
- 当单个客户端的待发送数据达到 256 KiB 时，服务端暂时停止读取该客户端的数据；队列降到 128 KiB 后恢复读取。若队列达到 1 MiB 硬上限，服务端会断开该慢客户端，避免其持续占用内存。
- `ChatService`：解析普通消息和聊天命令，生成消息投递结果。
- `ChatRoom`：维护文件描述符与用户名之间的映射，处理加入、离开、改名和用户查询。

服务端数据流：

```text
Socket 事件
    ↓
EpollServer
    ↓
ClientConnection 拆分消息
    ↓
ChatService 解析命令
    ↓
ChatRoom 查询或更新用户状态
    ↓
EpollServer 向目标客户端发送结果
```

### 客户端

`ChatClient` 使用 `poll` 监听：

- 标准输入 `STDIN_FILENO`
- 服务端 Socket 的可读事件
- 有待发送数据时的 Socket 可写事件
- Socket 关闭和错误事件

客户端 Socket 在连接成功后设置为非阻塞模式。无法一次发送完的数据会保留在发送缓冲区，等待下一次 `POLLOUT` 事件后继续发送。

### 日志

日志模块提供 `DEBUG`、`INFO`、`WARN` 和 `ERROR` 四个级别，默认输出 `INFO` 及以上日志。输出包含毫秒级时间戳：

```text
[2026-09-28 16:20:31.125] [INFO] server started port=8080
[2026-09-28 16:20:35.042] [INFO] client connected fd=8
[2026-09-28 16:20:42.517] [ERROR] accept4: Too many open files (errno=24)
```

日志输出由互斥锁保护，可以安全用于后续的多线程版本。`system_error()` 接收调用现场保存的 `errno`，用于记录系统调用名称、错误文本和错误编号。聊天正文仍写入标准输出，不与运行日志混合。

## 自动化测试

项目使用 CTest 管理测试。正常构建后执行：

```bash
ctest --test-dir build --output-on-failure
```

测试分为两部分：

- `chat_unit_tests`：覆盖 `ChatRoom`、`ChatService`、`ClientConnection` 和日志级别的核心行为、错误分支、消息拆分和缓冲区限制。
- `chat_integration_tests`：启动真实的 `EpollServer` 子进程，通过两个 TCP 客户端验证加入、广播、私聊、改名、用户列表和退出流程。

只运行单元测试：

```bash
ctest --test-dir build -R chat_unit_tests --output-on-failure
```

只运行集成测试：

```bash
ctest --test-dir build -R chat_integration_tests --output-on-failure
```

连续运行集成测试以检查稳定性：

```bash
ctest --test-dir build \
    -R chat_integration_tests \
    --repeat until-fail:20 \
    --output-on-failure
```

不需要测试时，可以关闭测试目标：

```bash
cmake -S . -B build -DBUILD_TESTING=OFF
```

## 压力测试

完整的测试环境、测试数据、结果分析和限制说明见 [性能与稳定性测试报告](docs/PERFORMANCE_REPORT.md)。

压力测试不属于默认 CTest，运行前需要先启动服务端：

```bash
./build/bin/server
```

然后在另一个终端执行：

```bash
python3 scripts/load_test.py \
    --host 127.0.0.1 \
    --port 8080 \
    --clients 100 \
    --messages 20
```

脚本会持续读取服务端广播，避免把正常吞吐测试误变成慢客户端测试，并输出连接数、发送消息数、接收数据量、发送速率、总耗时和错误数量。

验证慢客户端不会拖垮其他连接：

```bash
python3 scripts/slow_client_test.py --host 127.0.0.1 --port 8080
```

该脚本会建立一个不再读取数据的慢客户端和一个持续收发的正常客户端，制造足够多的广播数据，然后通过 `/list` 验证慢客户端已被移除、正常客户端仍然在线。

建议逐步增加负载：

```bash
python3 scripts/load_test.py --clients 10 --messages 10
python3 scripts/load_test.py --clients 50 --messages 20
python3 scripts/load_test.py --clients 100 --messages 20
```

聊天室会将一条群聊消息投递给所有在线用户，因此总投递量大约为 `客户端数 × 每客户端消息数 × 在线客户端数`。提高并发数前可以通过 `ulimit -n` 检查当前进程允许打开的文件描述符数量。

## 当前限制

- 仅支持 Linux。
- 仅支持 IPv4 数字地址，不支持域名解析和 IPv6。
- 服务端端口固定为 `8080`。
- 客户端服务器地址固定为 `127.0.0.1:8080`。
- 用户名和消息没有持久化，服务端退出后数据会丢失。
- 用户列表顺序不固定。
- 暂不支持身份认证、TLS 加密和消息历史记录。
- 当前协议返回的是面向终端显示的纯文本，不包含结构化消息类型或协议版本。

## 后续计划

- 通过命令行参数配置服务器地址和端口
- 支持域名解析与 IPv6
- 增加结构化协议、错误码和协议版本

## License

许可证信息请参见 [LICENSE](LICENSE)。
