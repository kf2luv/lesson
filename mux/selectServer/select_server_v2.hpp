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
static const size_t fdarray_size = sizeof(fd_set) * 8;
static const int defaultfd = -1;
static const size_t buf_size = 1024;

static const uint16_t DEFAULT_EVENT = 0X0;
static const uint16_t READ_EVENT = 0x1;
static const uint16_t WRITE_EVENT = 0x2;
static const uint16_t EXCEPT_EVENT = 0x4;

//模拟poll实现，只不过包含fd的结构体由上层封装
struct FdData
{
    FdData()
        : fd(defaultfd), port(0), event(DEFAULT_EVENT)
    {
    }
    int fd;
    std::string ip;
    uint16_t port;
    uint16_t event; // 要关心这个fd的就绪事件类型
    std::string echostr;
};
using FdType = FdData;

class SelectServer
{
public:
    SelectServer(uint16_t port = default_port) : port_(port), end_(0)
    {
    }
    ~SelectServer()
    {
        if (fdarray_)
            delete[] fdarray_;
    }

    void Init()
    {
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();
        fdarray_ = new FdData[fdarray_size];

        fdarray_[0].fd = listensock_.GetSockfd();
        fdarray_[0].event = READ_EVENT;
        ++end_;
    }

    void Start()
    {
        fd_set rfds;
        fd_set wfds;
        int maxfd = fdarray_[0].fd;

        while (true)
        {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            for (int i = 0; i < end_; i++)
            {
                if (fdarray_[i].fd != defaultfd)
                {
                    // 读事件设rfds, 写事件设wfds
                    if (fdarray_[i].event & READ_EVENT)
                        FD_SET(fdarray_[i].fd, &rfds);
                    if (fdarray_[i].event & WRITE_EVENT)
                        FD_SET(fdarray_[i].fd, &wfds);

                    maxfd = std::max(maxfd, fdarray_[i].fd);
                }
            }

            // struct timeval timeout = {2, 2};
            DebugPrint();
            int n = select(maxfd + 1, &rfds, &wfds, nullptr, nullptr);
            if (n == -1)
            {
                LogMessage(ERROR, "select error, errno: %d, strerror: %s\n", errno, strerror(errno));
                break;
            }
            else if (n == 0)
            {
                LogMessage(INFO, "not readfd ready!\n");
                continue;
            }
            else
            {
                // 读事件 | 写事件 就绪
                HandleEvent(rfds, wfds);
            }
        }
    }

    void HandleEvent(const fd_set &rfds2, const fd_set &wfds2) // 这里的rfds2和wfds2是经历select处理过的
    {
        // std::cout << "处理就绪事件" << std::endl;
        for (int i = 0; i < end_; i++)
        {
            if (fdarray_[i].fd == defaultfd)
                continue;
            if (fdarray_[i].event & READ_EVENT && FD_ISSET(fdarray_[i].fd, &rfds2))
            {
                // fd关心读事件, 且读事件就绪
                if (fdarray_[i].fd == listensock_.GetSockfd())
                {
                    AcceptNewLink();
                }
                else
                {
                    ServiceInput(i);
                }
            }
            if (fdarray_[i].event & WRITE_EVENT && FD_ISSET(fdarray_[i].fd, &wfds2))
            {
                // fd关心写事件, 且写事件就绪
                // ServiceOutput(i);
                ssize_t n = send(fdarray_[i].fd, fdarray_[i].echostr.c_str(), fdarray_[i].echostr.size(), 0);
                // 暂时不关心写事件了，等有数据要传给对端时再关心写事件
                fdarray_[i].event &= ~WRITE_EVENT;
            }
        }
    }

    void AcceptNewLink()
    {
        // 这里的Accept一定不会阻塞，因为listensock读事件就绪
        std::string client_ip;
        uint16_t client_port;
        int newfd = listensock_.Accept(&client_ip, &client_port);
        if (newfd < 0)
            return;

        // 将新的客户端fd存入fdarray中
        // 找到fdarray中的有效位置
        int pos = 0;
        while (pos < end_)
        {
            if (fdarray_[pos].fd == defaultfd)
                break;
            ++pos;
        }
        if (pos == end_)
        {
            if (end_ == fdarray_size)
            {
                // fdarray满载了
                close(newfd);
                LogMessage(ERROR, "accept fail: fdarray is fulled!\n");
                return;
            }
            else
            {
                pos = end_++;
            }
        }
        fdarray_[pos].fd = newfd;
        fdarray_[pos].ip = client_ip;
        fdarray_[pos].port = client_port;
        fdarray_[pos].event = READ_EVENT;

        LogMessage(DEBUG, "accept success: fdarray[%d]->newfd: %d\n", pos, newfd);
    }

    void ServiceInput(int fdpos)
    {
        int sockfd = fdarray_[fdpos].fd;
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
            fdarray_[fdpos].fd = defaultfd;
            return;
        }
        // 读取成功
        buf[n - 1] = 0;
        LogMessage(DEBUG, "[from %s:%d] %s\n", fdarray_[fdpos].ip.c_str(), fdarray_[fdpos].port, buf);

        // 发送消息回到客户端
        // 不应该在这里发, 写事件也应该就绪
        // 这里设置fd关心写事件
        fdarray_[fdpos].event |= WRITE_EVENT;

        std::string str = "echo# ";
        str += buf;
        fdarray_[fdpos].echostr = str;
    }

    void DebugPrint()
    {
        LogMessage(DEBUG, "current fdarray: ");
        for (int i = 0; i < end_; i++)
        {

            std::string flags;
            if (fdarray_[i].fd == defaultfd)
            {
                flags += "无效";
                continue;
            }
            if (fdarray_[i].event & READ_EVENT)
                flags += "R";
            if (fdarray_[i].event & WRITE_EVENT)
                flags += "W";

            printf("%d(%s) ", fdarray_[i].fd, flags.c_str());
        }
        std::cout << std::endl;
    }

private:
    uint16_t port_;
    Sock listensock_;
    FdType *fdarray_; // 存储服务器关心的fd
    size_t end_;      // fdarray的end, 规定end后面都是defaultfd, [0, end)至少有一个有效fd, 可以穿插defaultfd
};