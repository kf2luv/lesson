#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include "epoller.hpp"
#include "mysocket.hpp"
#include "util.hpp"
#include "protocol_netcal.hpp"

static const uint16_t defaultport = 8080;
static const int default_max = 64;
static const int buffersize = 1024;
static const time_t max_live_time = 5;

struct Connection;
using namespace protocol_ns_json;
using callback_t = std::function<void(Connection *)>; // 就绪事件处理函数，会用到Connection连接信息

// 存放每个连接的信息
struct Connection
{
    Connection(int fd, uint32_t events, callback_t recver, callback_t sender, callback_t excepter) // 三个callback，不需要的设nullptr
        : fd_(fd), events_(events), recver_(recver), sender_(sender), excepter_(excepter)
    {
        last_time_ = time(nullptr);
        pthread_mutex_init(mutex_, nullptr);
    }
    ~Connection()
    {
        pthread_mutex_destroy(mutex_);
    }

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

    // 最近访问时间
    time_t last_time_;
    pthread_mutex_t *mutex_;
};

// 本服务器默认都采用ET模式
class EpollServer
{
public:
    EpollServer(service_t service, uint16_t port = defaultport) : service_(service), port_(port)
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

    // 事件派发
    void Dispatch()
    {
        pthread_t pid;
        pthread_create(&pid, nullptr, DetectTimeout, this); // 超时检测器
        while (true)
        {
            int timeout = -1;
            LoopOnce(timeout);
        }
    }

    // 线程安全?
    static void *DetectTimeout(void *args)
    {
        pthread_detach(pthread_self());
        EpollServer *ts = static_cast<EpollServer *>(args);
        while (true)
        {
            for (auto &kv : ts->connections_)
            {
                int fd = kv.first;
                int cur_time = time(nullptr);
                int last_time = kv.second->last_time_;

                if (fd == ts->listensock_.GetSockfd())
                    continue;

                if (cur_time - last_time > max_live_time)
                {
                    LogMessage(DEBUG, "连接超时, fd: %d\n", fd);
                    // pthread_mutex_lock(kv.second->mutex_);
                    // kv.second->excepter_(kv.second);
                    // pthread_mutex_unlock(kv.second->mutex_);
                }
            }
        }
    }

    void LoopOnce(int timeout)
    {
        int maxevents = default_max;
        LogMessage(DEBUG, "waiting for epoll...\n");
        int readynum = epoller_.Wait(events_, maxevents, timeout);
        if (readynum == 0)
        {
            LogMessage(WARNING, "not fd ready\n");
            return;
        }
        HandleEvent(readynum);
    }

    // 关于异常
    // 有可能一个fd的读和写都就绪了, 在读的过程中，发生了异常，recver处理异常关闭连接，那么写的时候连接就不存在了
    // 因此Recv和Send之前，必须确保fd是存在的连接
    // fd可能在任何时候发生异常，这往往是由客户端决定的 (如客户端关闭连接)
    // 那么, 异常可能会被epoll捕捉到, 作为event通知给对应的fd; 另外, 异常也可能在Recv或Send的过程中直接出现
    // 统一?客户端断开链接，服务端这边会触发EPOLLIN (有些系统会触发EPOLLHUP, 但不通用)

    void HandleEvent(int readynum)
    {
        for (int i = 0; i < readynum; i++)
        {
            int fd = events_.GetFd(i);
            uint32_t events = events_.GetEvent(i);

            // 更新最近访问时间
            pthread_mutex_lock(connections_[fd]->mutex_);
            connections_[fd]->last_time_ = time(nullptr);
            pthread_mutex_unlock(connections_[fd]->mutex_);

            // fd的event事件已就绪

            if (events & EPOLLIN && ConnIsExist(fd))
            {
                LogMessage(DEBUG, "fd: %d, 读事件就绪\n", fd);
                connections_[fd]->recver_(connections_[fd]);
            }
            if (events & EPOLLOUT && ConnIsExist(fd))
            {
                LogMessage(DEBUG, "fd: %d, 写事件就绪\n", fd);
                connections_[fd]->sender_(connections_[fd]);
            }
            else if ((events & EPOLLERR || events & EPOLLHUP) && ConnIsExist(fd))
            {
                // 在epoller_.Wait检测到异常, 交给Recv和Send处理, 统一视为读取和写入的异常
                if (ConnIsExist(fd))
                    connections_[fd]->recver_(connections_[fd]);
                if (ConnIsExist(fd))
                    connections_[fd]->sender_(connections_[fd]);
            }
        }
    }

    // 连接管理
    void AddConnection(int fd, uint32_t events)
    {
        // 0.为ET模式作准备
        events |= EPOLLET;          // 注册fd事件为EPOLLET
        if (!util::SetNonBlock(fd)) // fd设为非阻塞, 因为ET模式的读写都是非阻塞的
        {
            LogMessage(WARNING, "SetNonBlock failed, errno: %d - strerror: %s\n", errno, strerror(errno));
            // 异常处理TODO
            return;
        }

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

    // 基于ET模式的就绪事件处理函数
    // 对于accept/read事件，一旦epoll通知就绪，必须把缓冲区中所有的数据读完

    void AcceptNewLink(Connection *conn)
    {
        // ET/LT都适用
        // ET只收到一次就绪通知，循环读，直到本次通知的所有数据读完
        // LT不循环读，因为只要缓冲区中有数据，它就会一直收到通知
        do
        {
            int newfd = listensock_.Accept();
            if (newfd < 0)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 非阻塞读，发现读到没有连接了
                {
                    // LogMessage(DEBUG, "AcceptNewLink: listen队列中已无连接, accept结束\n");
                    break;
                }
                else
                {
                    conn->excepter_(conn);
                    return;
                }
            }
            else
            {
                // accept成功, 添加一个新连接
                AddConnection(newfd, EPOLLIN);
            }
        } while (conn->events_ | EPOLLET);

        LogMessage(DEBUG, "AcceptNewLink: 本轮接收连接结束\n");
    }

    // ET模式，改造Recv, Send,
    void Recv(Connection *conn)
    {
        do
        {
            char buffer[buffersize] = {0};
            int recvnum = recv(conn->fd_, buffer, sizeof(buffer), 0);
            if (recvnum < 0)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 非阻塞读,发现读到没有数据了,表示本轮读取结束break
                    break;
                else
                {
                    conn->excepter_(conn);
                    return;
                }
            }
            else if (recvnum == 0)
            {
                // 对端关闭连接了
                LogMessage(DEBUG, "检测到对端关闭了连接, fd:%d\n", conn->fd_);
                conn->excepter_(conn);
                // 异常处理完毕, 不再读取, 直接返回
                return;
            }
            else
            {
                // read success
                buffer[recvnum] = 0;
                conn->inbuffer_ += buffer;
            }
        } while (conn->events_ | EPOLLET);

        LogMessage(DEBUG, "fd: %d, 本轮数据读取成功!\n", conn->fd_);

        // 一轮读取结束, 此时inbuffer中有一段字节流数据, 但不确定是否有完整的request报文
        // 接下来进行协议的分析 (网络版本计算器)
        // inbuffer中有多少个完整的request报文，就处理多少个，直到读不到完整的request报文，则退出，等待下次inbuffer新增数据

        // 处理数据的动作用工作线程来做?
        while (!conn->inbuffer_.empty())
        {
            std::string request;
            // Parse返回请求序列的有效载荷长度payload_len
            int plen = Parse(conn->inbuffer_, &request);
            if (plen == 0)
                break;
            else
            {
                // 此时已经有一个完整请求报文request
                LogMessage(DEBUG, "request: %s\n", request.c_str());
                std::string response = HandleRequest2Response(request, plen, service_);
                LogMessage(DEBUG, "response: %s\n", response.c_str());

                // 处理得到一个响应报文response, 直接发送!
                // send(conn->fd_, response.c_str(), response.size(), 0);
                conn->outbuffer_ += response;
                conn->sender_(conn);
            }
        }
    }

    // 关于写事件
    // 读事件是常设置的, 因为读事件就绪==接收缓冲区有数据, 大部分时间是不满足的, 要等对端发数据。
    // 而写事件不能常设置, 只能按需设置, 因为写事件就绪==发送缓冲区还有空间, 大部分时间都是满足的, 如果常设置会导致epoll频繁wait到写事件
    // Reactor策略:
    // 响应处理完毕后, 直接发送, 不用等待epoll告知写事件就绪 (首次发送时, 写事件必就绪)
    // 只有当发到发送缓冲区满了, 即写事件不再就绪, 此时设置fd关心写事件, 等待epoll告知下次写事件就绪

    void Send(Connection *conn)
    {
        int sentnum = 0;
        do
        {
            int num = conn->outbuffer_.size();                           // 预发送数
            sentnum = send(conn->fd_, conn->outbuffer_.c_str(), num, 0); // 实际发送数
            if (sentnum < 0)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 发送缓冲区已经满了
                {
                    EnableIO(conn->fd_, true, true);
                    break;
                }
                else
                {
                    conn->excepter_(conn);
                    return;
                }
            }
            else if (sentnum < num)
            {
                conn->outbuffer_.erase(0, sentnum);
                continue;
            }
            else
            {
                // send success
                conn->outbuffer_.erase(0, sentnum);
                break;
            }
        } while (conn->events_ | EPOLLET);

        LogMessage(DEBUG, "fd: %d, 本轮数据发送成功, 发送字节数: %d\n", conn->fd_, sentnum);
    }

    void HandleException(Connection *conn)
    {
        // 1.撤销内核epoll的管理
        epoller_.Remove(conn->fd_);
        // 2.删除connection集合中的映射关系
        connections_.erase(conn->fd_);
        // 3.关闭文件fd
        close(conn->fd_);
        // 4.删除conn对象
        LogMessage(DEBUG, "HandleException: 连接关闭了, fd: %d\n", conn->fd_);
        delete conn;
    }

    bool EnableIO(int fd, bool inable, bool outable)
    {
        if (!ConnIsExist(fd))
            return false;
        uint32_t events = (inable ? EPOLLIN : 0) | (outable ? EPOLLOUT : 0);
        epoller_.Modify(fd, connections_[fd]->events_ | events);
        return true;
    }

    bool ConnIsExist(int fd)
    {
        return connections_.find(fd) != connections_.end();
    }

private:
    uint16_t port_;                                     // 端口号
    Sock listensock_;                                   // 监听套接字
    Epoller epoller_;                                   // epoll模型
    Events events_;                                     // 就绪事件的获取等待类
    std::unordered_map<int, Connection *> connections_; // 存放连接的集合
    service_t service_;                                 // 业务逻辑处理函数
};

// 改良
// 1.要想从fd读取数据，必须满足两个条件：fd读事件就绪、fd缓冲区至少有一个完整报文。
// 同理，向fd写数据时，除了要fd写事件就绪，还要求已经有一个处理好的完整的响应报文
// 解决方法：为每个fd都设置两个缓冲区inbuffer和outbuffer，用于存放处理中的数据

// 2.每个连接，对应的就绪事件处理方法可能不一样，例如：listensock处理读事件是accept，普通fd是读取数据
// 为了代码可读性，解耦，为每个连接设置自己的就绪事件处理方法的回调函数，这样也更符合面向对象的思想啦

// TODO
// 3.加入应用层协议，便于理解 (和1相辅相成) OK

// 4.异常处理 OK

// 5.ET边缘触发模式 OK

// 6.连接管理 (时间): 设定每个连接如果长时间不通信，最多存活时间maxlivetime
// 给每一个Connection对象，加一个最近访问时间last_time，如果 cur_time - last_time > maxlivetime ，代表该连接超时了，应该删除