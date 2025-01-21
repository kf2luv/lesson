#pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include "epoller.hpp"
#include "mysocket.hpp"

static const uint16_t defaultport = 8080;
static const int default_max = 64;
static const int buffersize = 1024;

class EpollServer
{
public:
    EpollServer(uint16_t port = defaultport) : port_(port)
    {
    }
    ~EpollServer()
    {
    }

    void Init()
    {
        epoller_.Create();
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();

        epoller_.Register(listensock_.GetSockfd(), "r"); // 关心listensock的读事件
    }

    void Start()
    {
        while (true)
        {
            int maxevents = default_max;
            int timeout = -1;
            int readynum = epoller_.Wait(events_, maxevents, timeout);
            if (readynum == 0)
            {
                LogMessage(WARNING, "not fd ready\n");
                continue;
            }
            else
                HandleEvent(readynum);
        }
    }

    // void LoopOnce()
    // {

    // }

    void HandleEvent(int readynum)
    {
        LogMessage(DEBUG, "%d个fd就绪了\n", readynum);
        // 遍历整个events_, 对每个event做处理
        for (int i = 0; i < readynum; i++)
        {
            int fd = events_.GetFd(i);
            std::string event = events_.GetEvent(i);

            if (event == "r")
            {
                // 读事件就绪
                if (fd == listensock_.GetSockfd())
                    AcceptNewLink();
                else
                    Receive(fd);
            }
        }
    }

    void AcceptNewLink()
    {
        int newfd = listensock_.Accept();
        if (newfd < 0)
            return;
        epoller_.Register(newfd, "r");
    }

    void Receive(int fd)
    {
        char buf[buffersize];
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0)
        {
            LogMessage(ERROR, "recv fail, errno: %d - strerror: %s\n", errno, strerror(errno));
            return;
        }
        else if (n == 0)
        {
            LogMessage(WARNING, "client close, sockfd: %d\n", fd);
            // epoll remove的fd必须是有效的！不能先close
            // close(fd);
            // epoller_.Remove(fd);
            epoller_.Remove(fd);
            close(fd);
            return;
        }
        // 读取成功
        buf[n - 2] = 0;
        LogMessage(DEBUG, "read message: %s\r\n", buf);

        // send
        std::string echo = buf;
        echo += " [from echo server]\r\n";
        send(fd, echo.c_str(), echo.size(), 0);
    }

private:
    uint16_t port_;
    Sock listensock_;
    Epoller epoller_;
    Events events_;
};