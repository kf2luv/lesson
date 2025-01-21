#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <queue>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include "epoller.hpp"
#include "mysocket.hpp"
#include "util.hpp"
#include "protocol_netcal.hpp"
#include "thread_pool.hpp"

static const uint16_t defaultport = 8080;
static const int default_max = 64;
static const int buffersize = 1024;
static const time_t max_live_time = 5;
static const int service_thread_num = 3;

struct Connection;
using namespace protocol_ns_json;
using callback_t = std::function<void(Connection *)>; // 就绪事件处理函数，会用到Connection连接信息

#define LISTEN_YES 1
#define LISTEN_NO 0
#define RW_YES 1
#define RW_NO 0

// 存放每个连接的信息
struct Connection
{
    Connection(int fd, uint32_t events, callback_t recver, callback_t sender, callback_t excepter) // 三个callback，不需要的设nullptr
        : fd_(fd), events_(events), recver_(recver), sender_(sender), excepter_(excepter)
    {
    }
    ~Connection()
    {
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
};

class ServiceTask
{
public:
    ServiceTask(Connection *conn = nullptr, service_t s = nullptr) : conn_(conn), s_(s) {}
    ~ServiceTask() {}

    void operator()()
    {
        while (!conn_->inbuffer_.empty())
        {
            std::string request;
            // Parse返回请求序列的有效载荷长度payload_len
            int plen = Parse(conn_->inbuffer_, &request);
            if (plen == 0)
                break;
            else
            {
                // 此时已经有一个完整请求报文request
                LogMessage(DEBUG, "request: %s\n", request.c_str());
                std::string response = HandleRequest2Response(request, plen, s_);
                LogMessage(DEBUG, "response: %s\n", response.c_str());

                // 处理得到一个响应报文response, 直接发送!
                // send(conn->fd_, response.c_str(), response.size(), 0);
                conn_->outbuffer_ += response;
                conn_->sender_(conn_);
            }
        }
    }

private:
    Connection *conn_;
    service_t s_;
};

// 本服务器默认都采用ET模式
class Reactor
{
    // listenop区分epollServer是否携带listensock

public:
    Reactor(int listenop, int rwop, service_t service, uint16_t port = defaultport)
        : service_(service), port_(port), listenop_(listenop), rwop_(rwop)
    {
    }
    ~Reactor()
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
        if (listenop_ == LISTEN_YES)
        {
            listensock_.Socket();
            listensock_.Bind(port_);
            listensock_.Listen();
            AddConnection(listensock_.GetSockfd(), EPOLLIN);
        }
    }

    // 事件派发
    void Dispatch()
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
        // LogMessage(DEBUG, "waiting for epoll...\n");
        int readynum = epoller_.Wait(events_, maxevents, timeout);
        if (readynum == 0)
        {
            // LogMessage(WARNING, "not fd ready\n");
            return;
        }
        HandleEvent(readynum);
    }

    void HandleEvent(int readynum)
    {
        for (int i = 0; i < readynum; i++)
        {
            int fd = events_.GetFd(i);
            uint32_t events = events_.GetEvent(i);

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
        if (listenop_ == LISTEN_YES && fd == listensock_.GetSockfd())
        {
            // 不同类型的listensock有不同的处理方法
            if (rwop_ == RW_YES)
            {
                conn = new Connection(fd, events,
                                      std::bind(&Reactor::AcceptForMe, this, std::placeholders::_1), nullptr, nullptr);
            }

            else if (rwop_ == RW_NO)
            {
                conn = new Connection(fd, events,
                                      std::bind(&Reactor::AcceptForOther, this, std::placeholders::_1), nullptr, nullptr);
            }
        }

        else
        {
            conn = new Connection(fd, events,
                                  std::bind(&Reactor::Recv, this, std::placeholders::_1),
                                  std::bind(&Reactor::Send, this, std::placeholders::_1),
                                  std::bind(&Reactor::HandleException, this, std::placeholders::_1));
        }

        connections_[fd] = conn;
    }

    // 基于ET模式的就绪事件处理函数
    // 对于accept/read事件，一旦epoll通知就绪，必须把缓冲区中所有的数据读完

    void AcceptHelper(Connection *conn, int op)
    {
        do
        {
            int newfd = listensock_.Accept();
            if (newfd < 0)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 非阻塞读，发现读到没有连接了
                {
                    LogMessage(DEBUG, "连接accpet完了\n");
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
                if (op == 1)
                    AddConnection(newfd, EPOLLIN);
                else if (op == 2)
                {
                    outfds_.push(newfd);
                }
            }
        } while (conn->events_ | EPOLLET);
    }

    void AcceptForMe(Connection *conn)
    {
        AcceptHelper(conn, 1);
        LogMessage(DEBUG, "ListenAccept: 本轮接收连接结束\n");
    }

    // 推送获取到的连接fd, 不在本Reactor处理
    void AcceptForOther(Connection *conn)
    {
        AcceptHelper(conn, 2);
    }
    bool GetAcceptedFd(int *fd)
    {
        if (outfds_.empty())
            return false;

        *fd = outfds_.front();
        outfds_.pop();
        return true;
    }

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
        ThreadPool<ServiceTask>::get_instance(service_thread_num)->pushTask(ServiceTask(conn, service_));
        LogMessage(DEBUG, "线程池已接收当前业务\n");
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
        if (!ConnIsExist(conn->fd_))
            return;
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

    bool ConnsIsEmpty()
    {
        return connections_.empty();
    }

private:
    uint16_t port_;                                     // 端口号
    Sock listensock_;                                   // 监听套接字
    Epoller epoller_;                                   // epoll模型
    Events events_;                                     // 就绪事件的获取等待类
    std::unordered_map<int, Connection *> connections_; // 存放连接的集合
    service_t service_;                                 // 业务逻辑处理函数

    int listenop_;           // 是否携带listensock
    int rwop_;               // 是否在本reactor读写数据
    std::queue<int> outfds_; // 存放本reactor接收到的连接fd，一般是本reactor不处理数据IO，等待其它reacor接收的fd
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

// 6.连接管理(时间) NO TODO
// 设定每个连接如果长时间不通信，最多存活时间maxlivetime
// 给每一个Connection对象，加一个最近访问时间last_time，如果 cur_time - last_time > maxlivetime ，代表该连接超时了，应该删除

// 7.引入线程池, 进行业务处理
// Reactor只负责事件派发和数据读写，业务处理由线程池处理