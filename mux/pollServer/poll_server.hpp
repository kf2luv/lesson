#pragma once

#include <iostream>
#include <vector>
#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include "mysocket.hpp"

static const uint16_t default_port = 8080;
static const nfds_t default_max_nfds = 2;
static const int default_fd = -1;
static const size_t buf_size = 1024;

class PollServer;
using FdType = struct pollfd;
struct ThreadData
{
    ThreadData(int fdpos, char *buf, PollServer *psvr) : fdpos_(fdpos), buf_(buf), psvr_(psvr)
    {
    }

    int fdpos_;
    char *buf_;
    PollServer *psvr_;
};

class PollServer
{
public:
    PollServer(uint16_t port = default_port) : port_(port), end_(0), maxnfds_(default_max_nfds)
    {
        fds_ = new FdType[maxnfds_];
        for (int i = 0; i < maxnfds_; i++)
            fds_[i].fd = default_fd;
        outbuffer_.resize(maxnfds_, std::string());
    }
    ~PollServer()
    {
        if (fds_)
            delete[] fds_;
    }

    void Init()
    {
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();

        // 监听fd
        fds_[0].fd = listensock_.GetSockfd();
        fds_[0].events = POLLIN;
        ++end_;
    }

    void Start()
    {
        while (true)
        {
            int n = poll(fds_, end_, -1);
            if (n < 0)
            {
                LogMessage(ERROR, "poll error, errno: %d, strerror: %s\n", errno, strerror(errno));
                break;
            }
            else if (n == 0)
            {
                LogMessage(INFO, "Not fd is ready!\n");
                continue;
            }
            else
            {
                HandleEvent();
            }
        }
    }

    void HandleEvent()
    {
        // revent: 返回event中的就绪事件
        for (int i = 0; i < end_; i++)
        {
            if (fds_[i].fd == default_fd)
                continue;
            if (fds_[i].fd == listensock_.GetSockfd())
            {
                if (fds_[i].revents & POLLIN)
                    AcceptNewLink();
            }
            else // 非listensock的其它客户端fd
            {
                if (fds_[i].revents & POLLIN) // 读事件就绪
                    ServiceInput(i);
                if (fds_[i].revents & POLLOUT) // 写事件就绪
                {
                    // 满足发送的条件：写事件就绪，且有处理好的响应
                    if (!outbuffer_[i].empty())
                        ServiceOutput(i);
                }
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
        while (pos < end_)
        {
            if (fds_[pos].fd == default_fd)
                break;
            ++pos;
        }
        if (pos == end_)
        {
            if (end_ == maxnfds_)
            {
                // fds满载了, 扩容TODO
                // 1. 开辟新空间
                FdType *newfds = new FdType[2 * maxnfds_];
                // 2. 数据搬迁
                memcpy(newfds, fds_, maxnfds_ * sizeof(FdType));
                // 3. 成员变化
                end_ = maxnfds_;
                maxnfds_ *= 2;
                delete[] fds_;
                fds_ = newfds;
                // 4. 初始化新fds的后半段
                for (int i = end_; i < maxnfds_; i++)
                    fds_[i].fd = default_fd;

                LogMessage(DEBUG, "fds满载了, 扩容成功！\n");

                outbuffer_.resize(maxnfds_, std::string());
            }
            pos = end_++;
        }
        fds_[pos].fd = newfd;
        fds_[pos].events = POLLIN | POLLOUT;
        fds_[pos].revents = 0;

        LogMessage(DEBUG, "accept success: fds[%d]->newfd: %d\n", pos, newfd);
    }

    void ServiceInput(int fdpos)
    {
        int sockfd = fds_[fdpos].fd;
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
            fds_[fdpos].fd = default_fd;
            return;
        }
        // 读取成功
        buf[n - 1] = 0;
        LogMessage(DEBUG, "read message: %s\n", buf);

        // 读取到客户端数据后，服务器应用层进行业务处理
        // 业务处理完毕，得到一个响应
        // 此时可以将fd设为关心写事件
        // 等待fd写事件就绪即发送响应
        // 思路：用一个新线程处理业务，并为每一个fd设一个缓冲区，存放线程处理完毕的数据，等待写事件就绪开始输出
        pthread_t pid;
        pthread_create(&pid, nullptr, ThreadRoutine, new ThreadData(fdpos, buf, this));
    }
    static void *ThreadRoutine(void *args)
    {
        pthread_detach(pthread_self());
        ThreadData *td = static_cast<ThreadData *>(args);
        // echo业务处理
        std::string str = "echo# ";
        str += td->buf_;
        td->psvr_->outbuffer_[td->fdpos_] = str;
    }

    void ServiceOutput(int fdpos)
    {
        std::string &outstr = outbuffer_[fdpos];
        ssize_t n = send(fds_[fdpos].fd, outstr.c_str(), outstr.size(), 0);
        outstr.clear();
    }

private:
    uint16_t port_;
    Sock listensock_;
    FdType *fds_;
    nfds_t end_;
    nfds_t maxnfds_; // fds_的长度, 可以动态变化, 满了可以扩容

    std::vector<std::string> outbuffer_; // 与fds是一一对应的关系
};