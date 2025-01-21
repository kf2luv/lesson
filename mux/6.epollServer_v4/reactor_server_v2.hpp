#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include "log.hpp"
#include "Thread.hpp"
#include "Mutex.hpp"
#include "Cond.hpp"
#include "reactor.hpp"

static const int reactor_num = 3;

// class IoReactorTask
class ReactorServer;

struct ThreadData
{
    ThreadData(ReactorServer *rs, Reactor *rc, int index) : rs_(rs), rc_(rc), index_(index)
    {
    }
    ReactorServer *rs_;
    Reactor *rc_;
    int index_;
};

class ReactorServer
{
public:
    ReactorServer(service_t service, uint16_t port = defaultport)
        : listenReactor_(nullptr), iothreads_(nullptr), port_(port), service_(service)
    {
        listenReactor_ = new Reactor(LISTEN_YES, RW_NO, service, port);
        iothreads_ = new Thread[reactor_num];
        fds_.resize(sizeof(iothreads_));
        mcs_.resize(fds_.size());
    }
    ~ReactorServer()
    {
        if (listenReactor_)
            delete listenReactor_;
        if (iothreads_)
            delete[] iothreads_;
    }

    void Init()
    {
        listenReactor_->Init();
        for (int i = 0; i < reactor_num; i++)
        {
            iothreads_[i] = Thread(i + 1, ThreadRoutine, new ThreadData(this, nullptr, i + 1));
        }
    }

    void Start()
    {
        IoThreadStart();
        int timeout = -1;
        int index = 1; // 线程号从1开始
        while (true)
        {
            // 1.listenReactor等待accept新连接fd
            listenReactor_->LoopOnce(timeout);

            // 2.获取listenReactor的accept得到的fd
            int newfd = 0;
            while (listenReactor_->GetAcceptedFd(&newfd))
            {
                // 3.将newfd分配给某个线程中
                // 轮询分配
                LogMessage(DEBUG, "开始一次分配fd, fd: %d -> 线程: %d\n", newfd, index);
                {
                    lockGuard lg(&mcs_[index].first);
                    fds_[index].push(newfd);

                    // 唤醒等待fd分配的线程
                    mcs_[index].second.Wakeup();
                }

                index = (index + 1) % reactor_num;
            }
            // 分配fd结束后, 循环进行, listenReactor的工作就是不停的等待新连接, 有新连接就分配给线程

            // Reactor需要在新线程中持续维护, 线程池貌似不行, 因为线程池是处理短期任务为主的
        }
    }

    void IoThreadStart()
    {
        for (int i = 0; i < reactor_num; i++)
        {
            iothreads_[i].run();
        }
    }

    static void *ThreadRoutine(void *args)
    {
        pthread_detach(pthread_self());
        ThreadData *td = static_cast<ThreadData *>(args);
        Reactor *ioReactor = new Reactor(LISTEN_NO, RW_YES, td->rs_->service_, td->rs_->port_);
        ioReactor->Init();

        td->rc_ = ioReactor;
        Thread waitfd_thread(td->index_, WaitFdThreadRoutine, td);
        waitfd_thread.run();

        ioReactor->Dispatch();

        delete ioReactor;
        delete td;
    }

    static void *WaitFdThreadRoutine(void *args)
    {
        pthread_detach(pthread_self());
        ThreadData *td = static_cast<ThreadData *>(args);
        int timeout = -1;
        while (true)
        {
            // 阻塞等待分配新fd
            int newfd = td->rs_->WaitFd(td->index_, timeout);
            if (newfd > 0)
            {
                LogMessage(DEBUG, "线程: %d, 获取到一个fd\n", td->index_);
                td->rc_->AddConnection(newfd, EPOLLIN);
                LogMessage(DEBUG, "线程: %d, 已注册连接fd: %d\n", td->index_, newfd);
            }
        }
    }

    // wait_time == -1：阻塞等待
    // wait_time >= 0：非阻塞等待
    int WaitFd(int index, int wait_time)
    {

        // 对于fds_[index]的访问, 必须是主线程push之后, 其它线程再来pop
        // 而这样子加锁, 可能导致顺序不一致, 主线程即使有了新fd, 也因为无法竞争锁而无法pop新fd入容器中

        lockGuard lg(&mcs_[index].first);
        while (fds_[index].empty())
        {
            // LogMessage(DEBUG, "线程: %d, 正在等待fd分配\n", index);
            if (mcs_[index].second.Wait(mcs_[index].first, wait_time)) // 条件等待, 等待主线程分配fd
                continue;
            else
                return defaultfd;
        }

        int fd = fds_[index].front();
        fds_[index].pop();
        return fd;
    }

private:
    Reactor *listenReactor_;
    Thread *iothreads_; // 每个线程维护一个Reactor, 用于数据IO

    std::vector<std::queue<int>> fds_; // fds_[i]: 为第i号线程分配fd的队列 （加锁？同步变量？保证主线程和工作线程之间的线程安全和同步）
    std::vector<std::pair<Mutex, Cond>> mcs_;

    uint16_t port_;
    service_t service_;
};