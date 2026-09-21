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
│   ├── client
│   │   └── chat_client.h
│   └── server
│       ├── chat_room.h
│       ├── chat_service.h
│       ├── client_connection.h
│       └── epoll_server.h
├── src
│   ├── client
│   │   ├── chat_client.cpp
│   │   └── main.cpp
│   └── server
│       ├── chat_room.cpp
│       ├── chat_service.cpp
│       ├── client_connection.cpp
│       ├── epoll_server.cpp
│       └── main.cpp
├── CMakeLists.txt
├── LICENSE
└── README.md
```

## 代码结构

### 服务端

- `EpollServer`：创建监听 Socket，通过 `epoll` 接收连接并处理客户端读写事件。
- `ClientConnection`：保存单个客户端的接收缓冲区和发送缓冲区，处理半包与部分发送。
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
- 增加单元测试和端到端测试
- 增加信号处理和服务端优雅停机
- 支持域名解析与 IPv6
- 增加结构化协议、错误码和协议版本
- 增加日志级别和更完整的运行状态输出

## License

许可证信息请参见 [LICENSE](LICENSE)。
