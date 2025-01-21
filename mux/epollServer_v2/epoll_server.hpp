#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <unistd.h>
#include "epoller.hpp"
#include "mysocket.hpp"

static const uint16_t defaultport = 8080;
static const int default_max = 64;
static const int buffersize = 1024;

struct Connection;
using callback_t = std::function<void(Connection *)>; // 就绪事件处理函数，会用到Connection连接信息
// 存放每个连接的信息
struct Connection
{
    Connection(int fd, uint32_t events, callback_t recver, callback_t sender, callback_t excepter) // 三个callback，不需要的设nullptr
        : fd_(fd), events_(events), recver_(recver), sender_(sender), excepter_(excepter)
    {
    }
    ~Connection() {}

    // 连接信息
    int fd_;
    uint32_t events_;

    // 连接的输入输出缓冲区(用户级)
    std::string inbuffer_;
    std::string outbuffer_;

    // 就绪事件处理函数
    callback_t recver_;
    callback_t sender_;
    callback_t excepter_;
};

class EpollServer
{
public:
    EpollServer(uint16_t port = defaultport) : port_(port)
    {
    }
    ~EpollServer()
    {
        for (auto &kv : connections_)
        {
            if (kv.second)
                delete kv.second;
        }
    }

    void Init()
    {
        epoller_.Create();
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();

        AddConnection(listensock_.GetSockfd(), EPOLLIN);
    }

    void Start()
    {
        while (true)
        {
            int timeout = -1;
            LoopOnce(timeout);
        }
    }

    void LoopOnce(int timeout)
    {
        int maxevents = default_max;
        int readynum = epoller_.Wait(events_, maxevents, timeout);
        if (readynum == 0)
        {
            LogMessage(WARNING, "not fd ready\n");
            return;
        }
        else
            HandleEvent(readynum);
    }

    void HandleEvent(int readynum)
    {
        for (int i = 0; i < readynum; i++)
        {
            int fd = events_.GetFd(i);
            uint32_t events = events_.GetEvent(i);

            // fd的event事件已就绪
            if (events & EPOLLIN)
            {
                LogMessage(DEBUG, "fd: %d, 读事件就绪\n", fd);
                connections_[fd]->recver_(connections_[fd]);
            }
            if (events & EPOLLOUT)
            {
                LogMessage(DEBUG, "fd: %d, 写事件就绪\n", fd);
                connections_[fd]->sender_(connections_[fd]);
            }
            else if ((events & EPOLLERR) || (events & EPOLLHUP))
            {
                connections_[fd]->excepter_(connections_[fd]);
            }
        }
    }

    void AddConnection(int fd, uint32_t events)
    {
        // 1.向epoll模型中注册新的fd (内核)
        epoller_.Register(fd, events);

        // 2.添加新连接的信息 (用户层)
        Connection *conn;
        if (fd == listensock_.GetSockfd())
            conn = new Connection(fd, events,
                                  std::bind(&EpollServer::AcceptNewLink, this, std::placeholders::_1), nullptr, nullptr);
        else
            conn = new Connection(fd, events,
                                  std::bind(&EpollServer::Recv, this, std::placeholders::_1),
                                  std::bind(&EpollServer::Send, this, std::placeholders::_1),
                                  std::bind(&EpollServer::HandleException, this, std::placeholders::_1));

        connections_[fd] = conn;
    }

    void AcceptNewLink(Connection *conn)
    {
        int newfd = listensock_.Accept();
        if (newfd < 0)
            return;

        AddConnection(newfd, EPOLLIN);
        LogMessage(DEBUG, "新连接加入, fd: %d\n", newfd);
    }

    void Recv(Connection *conn)
    {
        char buf[buffersize];
        ssize_t n = recv(conn->fd_, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            LogMessage(WARNING, "读取fd: %d 出现异常!\n", conn->fd_);
            if (n < 0)
                epoller_.Modify(conn->fd_, conn->events_ | EPOLLERR);
            else if (n == 0)
                epoller_.Modify(conn->fd_, conn->events_ | EPOLLHUP);
            return;
        }

        // 读取成功
        buf[n - 2] = 0;
        LogMessage(DEBUG, "read message: %s\r\n", buf);

        // // send
        std::string echo = buf;
        echo += " [from echo server]\r\n";

        conn->outbuffer_ += echo;
        // 开始关心写事件
        epoller_.Modify(conn->fd_, conn->events_ | EPOLLOUT);
    }

    void Send(Connection *conn) // demo
    {
        send(conn->fd_, conn->outbuffer_.c_str(), conn->outbuffer_.size(), 0);
        conn->outbuffer_.clear();
        epoller_.Modify(conn->fd_, conn->events_ & ~EPOLLOUT);
    }

    void HandleException(Connection *conn)
    {
        if (conn->events_ & EPOLLERR)
        {
            LogMessage(ERROR, "#HandleException# recv fail, errno: %d - strerror: %s\n", errno, strerror(errno));
        }
        else if (conn->events_ & EPOLLHUP)
        {
            // EPOLLHUP
            LogMessage(WARNING, "#HandleException# client close, sockfd: %d\n", conn->fd_);
        }
        epoller_.Remove(conn->fd_);
        connections_.erase(conn->fd_);
        close(conn->fd_);
        delete conn;
    }

    // ET模式，改造Recv, Send,

private:
    uint16_t port_;                                     // 端口号
    Sock listensock_;                                   // 监听套接字
    Epoller epoller_;                                   // epoll模型
    Events events_;                                     // 就绪事件的获取等待类
    std::unordered_map<int, Connection *> connections_; // 存放连接的集合
};

// 改良
// 1.要想从fd读取数据，必须满足两个条件：fd读事件就绪、fd缓冲区至少有一个完整报文。
// 同理，向fd写数据时，除了要fd写事件就绪，还要求已经有一个处理好的完整的响应报文
// 解决方法：为每个fd都设置两个缓冲区inbuffer和outbuffer，用于存放处理中的数据

// 2.每个连接，对应的就绪事件处理方法可能不一样，例如：listensock处理读事件是accept，普通fd是读取数据
// 为了代码可读性，解耦，为每个连接设置自己的就绪事件处理方法的回调函数，这样也更符合面向对象的思想啦

// TODO
//  3.加入应用层协议，便于理解 (和1相辅相成)

// 4.异常处理??

// 5.ET边缘触发模式