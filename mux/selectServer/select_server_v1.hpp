#pragma once
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "mysocket.hpp"
#include "log.hpp"

static const uint16_t default_port = 8080;
static const size_t fdArraySize = sizeof(fd_set) * 8;
static const int defaultfd = -1;
static const size_t buf_size = 1024;

using FdType = int;

class SelectServer
{
public:
    SelectServer(uint16_t port = default_port) : port_(port), end_(0)
    {
    }

    void Init()
    {
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();

        fdarray_[0] = listensock_.GetSockfd();
        ++end_;
        for (int i = 1; i < fdArraySize; i++)
            fdarray_[i] = defaultfd;
    }

    void Start()
    {
        // 扫描当前进程维护的文件fd (从listensock开始)
        // 等待读事件就绪 (检测有没有读事件就绪的，如果有就处理，没有则继续)
        // 设置rfds

        fd_set rfds;
        int maxfd = fdarray_[0];

        while (true)
        {
            FD_ZERO(&rfds);
            for (int i = 0; i < end_; i++)
            {
                if (fdarray_[i] != defaultfd)
                {
                    FD_SET(fdarray_[i], &rfds);
                    maxfd = std::max(maxfd, fdarray_[i]);
                }
            }
            struct timeval timeout = {2, 2};
            // rfds
            // 用户->内核：告知内核“我”要关心的fd有哪些
            // 内核->用户：通知用户他要关心的fd中，哪些已就绪
            int n = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
            if (n == -1)
            {
                LogMessage(ERROR, "select error, errno: %d, strerror: %s\n", errno, strerror(errno));
                break;
            }
            else if (n == 0)
            {
                //无fd读事件就绪
                LogMessage(INFO, "not readfd ready!\n");
                continue;
            }
            else
            {
                //有fd读事件就绪，进入处理函数
                HandleEvent(rfds);
            }
        }
    }

    void HandleEvent(const fd_set &rfds2) // 这里的rfds2是经历select处理过的，其中包含已就绪的fd
    {
        for (int i = 0; i < end_; i++)
        {
            if (FD_ISSET(fdarray_[i], &rfds2))
            {
                if (fdarray_[i] == listensock_.GetSockfd())
                    // listensock就绪，表示有新连接到来，accept
                    AcceptNewLink();
                else
                    // 客户端sock就绪，读取数据，执行服务
                    ServiceIO(i);
            }
        }
    }

    void AcceptNewLink()
    {
        // 这里的Accept一定不会阻塞，因为listensock读事件就绪
        int newfd = listensock_.Accept();
        if (newfd < 0)
            return;

        // 将新的客户端fd存入fdarray中
        // 找到fdarray中的有效位置
        int pos = 0;
        while (pos < end_ && fdarray_[pos] != defaultfd)
        {
            ++pos;
        }
        if (pos == end_)
        {
            if (end_ == fdArraySize)
            {
                // fdarray满载了
                close(newfd);
                LogMessage(ERROR, "accept fail: fdarray is fulled!\n");
                return;
            }
            ++end_;
        }
        fdarray_[pos] = newfd;
        LogMessage(DEBUG, "accept success: fdarray[%d]->newfd: %d\n", pos, newfd);

        LogMessage(DEBUG, "current fdarray: ");
        for (int i = 0; i < end_; i++)
            std::cout << fdarray_[i] << " ";
        std::cout << std::endl;
    }

    void ServiceIO(int fdpos)
    {
        int sockfd = fdarray_[fdpos];
        char buf[buf_size] = {0};
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
        if (n < 0)
        {
            LogMessage(ERROR, "recv fail, errno: %d, strerror: %s\n", errno, strerror(errno));
            return;
        }
        else if (n == 0)
        {
            LogMessage(WARNING, "client close, sockfd: %d\n", sockfd);
            close(sockfd);
            fdarray_[fdpos] = defaultfd;
            return;
        }
        // 读取成功
        buf[n] = 0;
        LogMessage(DEBUG, "[from...] %s\n", buf);

        // 发送消息回到客户端
        std::string echo_str = "echo# ";
        echo_str += buf;
        n = send(sockfd, echo_str.c_str(), echo_str.size(), 0);
    }

private:
    uint16_t port_;
    Sock listensock_;
    FdType fdarray_[fdArraySize]; // 存储服务器关心的fd
    size_t end_;                   // fdarray的end, 规定end后面都是defaultfd, [0, end)至少有一个有效fd, 可以穿插defaultfd
};