#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include "log.hpp"
#include "Thread.hpp"
#include "Mutex.hpp"
#include "Cond.hpp"
#include "reactor.hpp"

static const int reactor_num = 5;

// class IoReactorTask
class ReactorServer;

struct ThreadData
{
    ThreadData(ReactorServer *rs, int index) : rs_(rs), index_(index)
    {
    }
    ReactorServer *rs_;
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
            iothreads_[i] = Thread(i + 1, ThreadRoutine, new ThreadData(this, i + 1));
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
        // 1.创建属于该线程的Reactor, 用于数据IO
        Reactor *ioReactor = new Reactor(LISTEN_NO, RW_YES, td->rs_->service_, td->rs_->port_);
        ioReactor->Init();

        int timeout = 1000;
        // 这里线程要进行两个等，一等主线程分配fd，二等现有fd事件就绪
        // 两个等都需要“别人”通知，一等要主线程通知，二等要epoller通知
        // 如果是阻塞等待，有如下BUG：
        // 线程已经存在fd，在“一等”的过程中，fd事件就绪，但是主线程一直不分配fd，“一等”迟迟不结束，fd就绪事件也一直无法处理

        // 因此，两个等都不能阻塞等待，而是非阻塞
        // 每次等待就等一小会，等到了，就进行对应的处理动作，等不到，就超时退出，进行另一个等待
        while (true)
        {
            // 2.等待主线程分配fd

            // 线程没有fd时，就要阻塞等待到有fd分配为止
            // 线程至少有一个fd时，等一会fd分配就行，等不到就去处理就绪事件了
            int newfd = 0;
            int wait_time = timeout / 1000;
            if (ioReactor->ConnsIsEmpty())
            {
                wait_time = -1;
                LogMessage(DEBUG, "线程: %d, 阻塞等待fd分配中\n", td->index_);
            }

            newfd = td->rs_->WaitFd(td->index_, wait_time); // 等待单位是秒
            // 3.拿到了一个fd, 建立新连接
            if (newfd > 0)
            {
                LogMessage(DEBUG, "线程: %d, 获取到一个fd\n", td->index_);
                ioReactor->AddConnection(newfd, EPOLLIN);
                LogMessage(DEBUG, "线程: %d, 已注册连接fd: %d\n", td->index_, newfd);
            }

            // 4.等待现有fd的事件就绪, 并处理就绪事件
            ioReactor->LoopOnce(timeout); // 等待单位是毫秒

            // detect timeout
        }

        // 线程刚开始运行（首次循环），一定是没有fd的，必须先等待主线程分配至少一个fd
        // 线程获取到第一个fd后，就可以等待现有fd的事件就绪, 并处理就绪事件（Looponce）
        // Looponce有超时时间，时间一到，内部可能处理了就绪事件，也有可能在这段时间内根本没有就绪事件
        // 因此，Looponce结束之后，不管什么情况，进行一次超时探测，对于长时间没有访问的fd，断开连接
        // Loop结束后，进行下次等待分配fd

        /*超时探测 TODOOOOOOOOOOOOOOOOOO*/

        delete td;
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