#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
#include <algorithm>
#include <csignal>
#include <cerrno>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/epoll.h>

constexpr int BufSize = 1024;
constexpr int MaxEvents = 4096;
constexpr int BackLog = 4096;
const char *LocalHost = "127.0.0.1";

constexpr int SelectPort = 8888;
constexpr int PollPort = 8889;
constexpr int EpollPort = 8890;

// ---------- 提升进程文件描述符上限 ----------
static void raise_fd_limit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("getrlimit()");
        return;
    }
    rlim_t cur = rl.rlim_cur;
    rlim_t max = rl.rlim_max;
    rlim_t target = std::min(rl.rlim_max, (rlim_t)65536);
    printf("[fd] limit: soft=%lu, hard=%lu",
           (unsigned long)cur, (unsigned long)max);

    if (cur >= target) {
        printf(" (already sufficient)\n");
        return;
    }

    // 尝试将 soft 提升到 65536 或 hard 上限
    rl.rlim_cur = target;
    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
        // 降级尝试：升到 hard 上限
        rl.rlim_cur = rl.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
            printf(" -> WARNING: setrlimit() failed (%s)\n", strerror(errno));
            printf("[fd] ⚠ 当前 soft=%lu, 最多处理约 %lu 并发连接\n",
                   (unsigned long)cur, (unsigned long)cur - 10);
            printf("[fd] 🔧 测试前运行: ulimit -n 65536\n");
            return;
        }
    }
    getrlimit(RLIMIT_NOFILE, &rl);
    printf(" -> soft=%lu (max concurrent ≈ %lu)\n",
           (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_cur - 10);
}

int server_poll(int argc, char *argv[]) {
    raise_fd_limit();
    // 1. 创建非阻塞 socket
    int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 设置地址复用，防止端口被占用
    int opt = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsocketopt()");
        exit(1);
    }
    // 2. 绑定地址
    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = htonl(INADDR_ANY);
    laddr.sin_port = htons(PollPort);
    if (bind(sd, reinterpret_cast<const sockaddr *>(&laddr), sizeof(laddr)) < 0) {
        perror("bind()");
        exit(1);
    }
    // 3. 监听
    if (listen(sd, BackLog) < 0) {
        perror("listen()");
        exit(1);
    }
    // 4. 初始化fd 数组
    struct pollfd fds[MaxEvents];
    int nfds = 0; // 当前数组中有效元素的个数
    // 将监听 socket 加入
    fds[0].fd = sd;
    fds[0].events = POLLIN;
    nfds = 1;

    printf("Echo server[io/poll] started ...\n");

    while (true) {
        // 调用 poll ，无限等待
        if (poll(fds, nfds, -1) < 0) {
            perror("poll()");
            exit(1);
        }
        // 遍历所有的已经注册的 fd (注意：ndfs 是当前数组的长度）
        for (int i = 0; i < nfds; ++i) {
            if (fds[i].revents == 0)
                continue; // 无事件

           if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
               // 关闭该连接并移出pollfd
               if (fds[i].fd != sd) {
                   close(fds[i].fd);
                   printf("Client fd %d closed due to error\n", fds[i].fd);
               }
               // 标记为无效，稍后压缩
               fds[i].fd = -1;
               continue;
           }
           // --- 监听 socket 可读：新连接（循环 accept 直到 EAGAIN） ---
           if (fds[i].fd == sd && (fds[i].revents & POLLIN)) {
                while (true) {
                    struct sockaddr_in raddr;
                    socklen_t raddr_len = sizeof(raddr);
                    int newsd = accept4(sd, reinterpret_cast<sockaddr *>(&raddr),
                                        &raddr_len, SOCK_NONBLOCK);
                    if (newsd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept()");
                        break;
                    }
                    // 禁用 Nagle 算法
                    int flag = 1;
                    setsockopt(newsd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    // 查找第一个空闲的位置(fd == -1),如没有则追加
                    int slot = -1;
                    for (int j = 0; j < nfds; ++j) {
                        if (fds[j].fd == -1) {
                            slot = j;
                            break;
                        }
                    }
                    if (slot == -1) {
                        if (nfds >= MaxEvents) {
                            close(newsd);
                            fprintf(stderr, "Too many connections, rejected!\n");
                            break;
                        }
                        slot = nfds++;
                    }
                    fds[slot].fd = newsd;
                    fds[slot].events = POLLIN;
                    printf("New client connected: %d\n", newsd);
                }
           }
           // ---- 客户端 socket 可读（循环 non-blocking read 直到 EAGAIN） ----
           else if (fds[i].fd != sd && (fds[i].revents & POLLIN)) {
                char buf[BufSize];
                bool closed = false;

                while (true) {
                    ssize_t n = read(fds[i].fd, buf, BufSize);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("read()");
                        closed = true;
                        break;
                    }
                    if (n == 0) {
                        closed = true;
                        break;
                    }
                    // --- 响应客户端 ---
                    bool write_err = false;
                    const char *response = nullptr;
                    ssize_t resp_len = 0;
                    if (strcmp(argv[argc-1], "redis") != 0) {
                        response = buf;
                        resp_len = n;
                    } else {
                        response = "+PONG\r\n";
                        resp_len = 7;
                    }
                    // 部分写入循环
                    ssize_t remaining = resp_len;
                    const char *ptr = response;
                    while (remaining > 0) {
                        ssize_t wn = write(fds[i].fd, ptr, remaining);
                        if (wn < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                write_err = true;
                            } else {
                                write_err = true;
                            }
                            break;
                        }
                        ptr += wn;
                        remaining -= wn;
                    }
                    if (write_err) {
                        closed = true;
                        break;
                    }
                }

                if (closed) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    printf("Client %d disconnected\n", fds[i].fd);
                }
           }
        }

        // 压缩数组：移出 fd == -1 的空闲位置，保持紧凑
        int k = 0;
        for (int i = 0; i < nfds; ++i) {
            if (fds[i].fd != -1) {
                if (i != k) fds[k] = fds[i];
                ++k;
            }
        }
        nfds = k;
    }

    close(sd);
    return 0;
}

int client_poll(int argc, char *argv[]) {
    // 1. 创建 socket
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 2. 连接服务器
    struct sockaddr_in raddr;
    std::memset(&raddr, 0, sizeof(raddr));
    raddr.sin_family = AF_INET;
    raddr.sin_addr.s_addr = inet_addr(LocalHost);
    raddr.sin_port = htons(PollPort);
    if (connect(sd, reinterpret_cast<const sockaddr *>(&raddr), sizeof(raddr)) < 0) {
        perror("connect()");
        exit(1);
    }

    printf("Connected to echo server[io/poll] %s:%d\n", LocalHost, PollPort);

    // 3. 初始化 pollfd 数组： 监控 stdin(0) 和 socket
    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[1].fd = sd;
    fds[1].events = POLLIN;
    char buf[BufSize];

    while (true) {
        if (poll(fds, 2, -1) < 0) {
            perror("poll()");
            exit(1);
        }
        // ---- 标准输入可读： 用户输入 ----
        if (fds[0].revents & POLLIN) {
            fgets(buf, BufSize, stdin);
            // 去除换行符
            buf[strcspn(buf, "\n")] = '\0';
            // 如果输入 quit 或 exit 则退出
            if (strcmp(buf, "quit") == 0
                || strcmp(buf, "exit") == 0
                || strcmp(buf, "q") == 0) {
                printf("Exiting...\n");
                break;
            }
            // 发送给服务器
            write(sd, buf, strlen(buf));
        }
        // ---- 服务器 socket 可读：收到回声 ----
        if (fds[1].revents & POLLIN) {
            ssize_t n = read(sd, buf, BufSize);
            if (n == 0) {
                printf("Server[io/poll] closed connection.\n");
                break;
            }
            buf[n] = '\0';
            printf("Echo from server[io/poll]: %s\n", buf);
        }
    }

    close(sd);
    return 0;
}




// ./iomultiplexing s redis
int server_select(int argc, char *argv[]) {
    raise_fd_limit();
    // 1. 创建非阻塞监听 socket
    int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 2. 设置地址复用
    int opt = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 3. 绑定地址和端口
    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = htonl(INADDR_ANY);
    laddr.sin_port = htons(SelectPort);

    if (bind(sd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
        perror("bind()");
        exit(1);
    }
    // 4. 监听
    if (listen(sd, BackLog) < 0) {
        perror("listen()");
        exit(1);
    }
    // 5. 初始化 select 的 fd_set
    fd_set read_fds;
    fd_set ready_fds;
    FD_ZERO(&read_fds);
    FD_SET(sd, &read_fds);
    int fd_max = sd;

    printf("Echo server[io/select] started ...\n");
    char buf[BufSize];

    while (true) {
        // 每次循环复制一份，因为 select 会修改集合
        ready_fds = read_fds;
        // 调用 select, 无限等待
        int fd_num = select(fd_max + 1, &ready_fds, NULL, NULL, NULL);
        if (fd_num < 0) {
            perror("select()");
            exit(1);
        }
        // 遍历所有可能的文件描述符
        for (int i = 0; i <= fd_max; ++i) {
            if (!FD_ISSET(i, &ready_fds))
                continue;
            // --- 监听 socket 有事件：新连接到来（循环 accept 直到 EAGAIN）---
            if (i == sd) {
                while (true) {
                    struct sockaddr_in raddr;
                    socklen_t addr_size = sizeof(raddr);
                    int newsd = accept4(sd, reinterpret_cast<struct sockaddr *>(&raddr),
                                        &addr_size, SOCK_NONBLOCK);
                    if (newsd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept()");
                        break;
                    }
                    // 禁用 Nagle 算法
                    int flag = 1;
                    setsockopt(newsd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    // 将新客户端加入监视集合
                    FD_SET(newsd, &read_fds);
                    fd_max = std::max(fd_max, newsd);
                    printf("New client connected: %d\n", newsd);
                }
            }
            else {
                // ---- 客户端 socket 可读（循环 non-blocking read 直到 EAGAIN）----
                bool closed = false;

                while (true) {
                    ssize_t n = read(i, buf, BufSize);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("read()");
                        closed = true;
                        break;
                    }
                    if (n == 0) {
                        closed = true;
                        break;
                    }
                    // --- 响应客户端 ---
                    bool write_err = false;
                    const char *response = nullptr;
                    ssize_t resp_len = 0;
                    if (strcmp(argv[argc - 1], "redis") == 0) {
                        response = "+PONG\r\n";
                        resp_len = 7;
                    } else {
                        response = buf;
                        resp_len = n;
                    }
                    // 部分写入循环
                    ssize_t remaining = resp_len;
                    const char *ptr = response;
                    while (remaining > 0) {
                        ssize_t wn = write(i, ptr, remaining);
                        if (wn < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                write_err = true;
                            } else {
                                write_err = true;
                            }
                            break;
                        }
                        ptr += wn;
                        remaining -= wn;
                    }
                    if (write_err) {
                        closed = true;
                        break;
                    }
                }

                if (closed) {
                    FD_CLR(i, &read_fds);
                    close(i);
                    printf("Client disconnected: %d\n", i);
                }
            }
        }
    }

    close(sd);
    return 0;
}


int client_select(int argc, char *argv[]) {
    // 1. 创建 socket
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 2. 连接服务器
    struct sockaddr_in raddr;
    memset(&raddr, 0, sizeof(raddr));
    raddr.sin_family = AF_INET;
    raddr.sin_addr.s_addr = inet_addr(LocalHost);
    raddr.sin_port = htons(SelectPort);
    if (connect(sd, reinterpret_cast<const sockaddr *>(&raddr), sizeof(raddr)) < 0) {
        perror("connect()");
        exit(1);
    }

    printf("Connected to echo server[io/select] %s:%d\n", LocalHost, SelectPort);
    // 3. 初始化 select 监视集合： 标准输入0 + sd
    fd_set  read_fds;
    fd_set  ready_fds;
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    FD_SET(sd, &read_fds);
    int fd_max = sd;
    char buf[BufSize];

    while (true) {
        ready_fds = read_fds;
        int fd_num = select(fd_max + 1, &ready_fds, NULL, NULL, NULL);
        if (fd_num < 0) {
            perror("select()");
            exit(1);
        }
        // --- 标准输入有数据：用户输入了消息 ----
        if (FD_ISSET(0, &ready_fds)) {
            fgets(buf, BufSize, stdin);
            // 去除换行符
            buf[strcspn(buf, "\n")] = '\0';
            // 如果输入 quit 或 exit 则退出
            if (strcmp(buf, "quit") == 0
                    || strcmp(buf, "exit") == 0
                    || strcmp(buf, "q") == 0) {
                printf("Exiting...\n");
                break;
            }
            // 发送给服务器
            write(sd, buf, strlen(buf));
        }
        // --- 服务器 socket 有数据： 收到回声 ----
        if (FD_ISSET(sd, &ready_fds)) {
            int str_len = read(sd, buf, BufSize);
            if (str_len == 0) {
                printf("Server[io/select] close connection.\n");
                break;
            }
            buf[str_len] = '\0';
            printf("Echo from server[io/select]: %s\n", buf);
        }
    }

    close(sd);
    return  0;
}

int server_epoll(int argc, char *argv[]) {
    raise_fd_limit();
    // 1. 创建监听 socket（直接设为非阻塞）
    int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 设置地址复用
    int opt = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt()");
        exit(1);
    }
    // 2. 绑定地址
    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = htonl(INADDR_ANY);
    laddr.sin_port = htons(EpollPort);
    if (bind(sd, reinterpret_cast<const sockaddr *>(&laddr), sizeof(laddr)) < 0) {
        perror("bind()");
        exit(1);
    }
    // 3. 监听
    if (listen(sd, BackLog) < 0) {
        perror("listen()");
        exit(1);
    }
    // 4. 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1()");
        exit(1);
    }
    // 5. 将监听 socket 加入 epoll（LT 模式即可，适合 listen fd）
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &event) < 0) {
        perror("epoll_ctl()");
        exit(1);
    }

    printf("Echo server[io/epoll] started ...\n");

    struct epoll_event events[MaxEvents];

    while (true) {
        int nfds = epoll_wait(epfd, events, MaxEvents, -1);
        if (nfds < 0) {
            perror("epoll_wait()");
            exit(1);
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            // ---- 处理错误/挂起事件 ----
            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if (fd != sd) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    printf("Client fd %d closed (event 0x%x)\n", fd, ev);
                }
                continue;
            }

            // === 监听 socket：接受新连接（ET 模式：循环 accept 直到 EAGAIN） ===
            if (fd == sd && (ev & EPOLLIN)) {
                while (true) {
                    struct sockaddr_in raddr;
                    socklen_t raddr_len = sizeof(raddr);
                    int newsd = accept4(sd, reinterpret_cast<sockaddr *>(&raddr),
                                        &raddr_len, SOCK_NONBLOCK);
                    if (newsd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;  // 没有更多新连接
                        perror("accept()");
                        break;
                    }
                    // 禁用 Nagle 算法，减少延迟
                    int flag = 1;
                    setsockopt(newsd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    // ET 边沿触发模式——epoll 性能关键
                    struct epoll_event new_event;
                    new_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    new_event.data.fd = newsd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, newsd, &new_event) < 0) {
                        perror("epoll_ctl() add client");
                        close(newsd);
                        continue;
                    }
                    printf("New client connected: %d\n", newsd);
                }
            }
            // === 客户端 socket：数据可读（ET 模式：循环 read 直到 EAGAIN） ===
            else if (fd != sd && (ev & EPOLLIN)) {
                char buf[BufSize];
                bool closed = false;

                while (true) {
                    ssize_t n = read(fd, buf, BufSize);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;  // 数据已全部读完
                        // 真实错误
                        perror("read()");
                        closed = true;
                        break;
                    }
                    if (n == 0) {
                        // 客户端关闭
                        closed = true;
                        break;
                    }
                    // --- 响应客户端 ---
                    bool write_err = false;
                    if (strcmp(argv[argc-1], "redis") != 0) {
                        // 普通回声：原样返回
                        const char *ptr = buf;
                        ssize_t remaining = n;
                        while (remaining > 0) {
                            ssize_t wn = write(fd, ptr, remaining);
                            if (wn < 0) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    // 写入缓冲区满——简化处理，直接断开
                                    // 生产环境应注册 EPOLLOUT 等待可写
                                    write_err = true;
                                    break;
                                }
                                write_err = true;
                                break;
                            }
                            ptr += wn;
                            remaining -= wn;
                        }
                        if (write_err) {
                            closed = true;
                            break;
                        }
                    }
                    else {
                        // Redis PING 协议响应
                        const char pong[] = "+PONG\r\n";
                        ssize_t remaining = sizeof(pong) - 1;
                        const char *ptr = pong;
                        while (remaining > 0) {
                            ssize_t wn = write(fd, ptr, remaining);
                            if (wn < 0) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    write_err = true;
                                    break;
                                }
                                write_err = true;
                                break;
                            }
                            ptr += wn;
                            remaining -= wn;
                        }
                        if (write_err) {
                            closed = true;
                            break;
                        }
                    }
                }

                if (closed) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    printf("Client %d disconnected\n", fd);
                }
            }
        }
    }

    close(epfd);
    close(sd);
    return 0;
}

int client_epoll(int argc, char *argv[]) {
    // 1. 创建 socket
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
    // 2. 连接服务器
    struct sockaddr_in raddr;
    std::memset(&raddr, 0, sizeof(raddr));
    raddr.sin_family = AF_INET;
    raddr.sin_addr.s_addr = inet_addr(LocalHost);
    raddr.sin_port = htons(EpollPort);
    if (connect(sd, reinterpret_cast<const sockaddr *>(&raddr), sizeof(raddr)) < 0) {
        perror("connect()");
        exit(1);
    }

    printf("Connected to echo server[io/epoll] %s:%d\n", LocalHost, EpollPort);

    // 3. 初始化 epoll：监控 stdin(0) 和 socket
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1()");
        exit(1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = 0;  // 标准输入
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev) < 0) {
        perror("epoll_ctl() stdin");
        exit(1);
    }
    ev.data.fd = sd;  // 服务器连接
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &ev) < 0) {
        perror("epoll_ctl() socket");
        exit(1);
    }

    struct epoll_event events[2];
    char buf[BufSize];

    while (true) {
        int nfds = epoll_wait(epfd, events, 2, -1);
        if (nfds < 0) {
            perror("epoll_wait()");
            exit(1);
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t evt = events[i].events;

            if (evt & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                printf("Connection closed (fd=%d)\n", fd);
                goto cleanup;
            }

            if (fd == 0 && (evt & EPOLLIN)) {
                // 标准输入可读：用户输入
                fgets(buf, BufSize, stdin);
                buf[strcspn(buf, "\n")] = '\0';
                if (strcmp(buf, "quit") == 0
                    || strcmp(buf, "exit") == 0
                    || strcmp(buf, "q") == 0) {
                    printf("Exiting...\n");
                    goto cleanup;
                }
                write(sd, buf, strlen(buf));
            }

            if (fd == sd && (evt & EPOLLIN)) {
                // 服务器 socket 可读：收到回声
                ssize_t n = read(sd, buf, BufSize);
                if (n <= 0) {
                    printf("Server[io/epoll] closed connection.\n");
                    goto cleanup;
                }
                buf[n] = '\0';
                printf("Echo from server[io/epoll]: %s\n", buf);
            }
        }
    }

cleanup:
    close(epfd);
    close(sd);
    return 0;
}


void test_select(int argc, char *argv[]) {
    if (strcmp(argv[2], "s") == 0) {
        server_select(argc, argv);
    }
    else if (strcmp(argv[2], "c") == 0) {
        client_select(argc, argv);
    }
    else {
        printf("Please confirm server[s] or client[c].\n");
        exit(1);
    }
}

void test_poll(int argc, char *argv[]) {
    if (strcmp(argv[2], "s") == 0) {
        server_poll(argc, argv);
    }
    else if (strcmp(argv[2], "c") == 0) {
        client_poll(argc, argv);
    }
    else {
        printf("Please confirm server[s] or client[c].\n");
        exit(1);
    }
}


void test_epoll(int argc, char *argv[]) {
    if (strcmp(argv[2], "s") == 0) {
        server_epoll(argc, argv);
    }
    else if (strcmp(argv[2], "c") == 0) {
        client_epoll(argc, argv);
    }
    else {
        printf("Please confirm server[s] or client[c].\n");
        exit(1);
    }
}





int main(int argc, char *argv[])
{
    if (argc < 2) {
        // 启动redis-benchmark 测试: ./test_iomultiplexing poll s redis
        // 启动普通回声服务器服务端：./test_iomultiplexing poll s
        // 启动普通回声服务器客户端：./test_iomultiplexing poll c
        printf("Usage: %s <type> <s/c> [redis可选]\n", argv[0]);
        exit(1);
    }
    if (strcmp(argv[1], "select") == 0) {
        test_select(argc, argv);
    }
    else if (strcmp(argv[1], "poll") == 0) {
        test_poll(argc, argv);
    }
    else if (strcmp(argv[1], "epoll") == 0) {
        test_epoll(argc, argv);
    }
    else {
        printf("Usage: %s <type> <s/c> [redis可选]\n", argv[0]);
        exit(1);
    }
    return 0;
}