#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <map>

#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>
#include "log.hpp"
#include "err.hpp"

static const int epollsize = 64;
static const int defaultfd = -1;
static const int gsize = 1024;

static std::unordered_map<std::string, uint32_t> myevents = {{"r", EPOLLIN}, {"w", EPOLLOUT}, {"rw", EPOLLIN | EPOLLOUT}};

class Events
{
public:
    Events()
    {
        events_ = new struct epoll_event[gsize];
    }
    ~Events()
    {
        if (events_)
            delete[] events_;
    }

    struct epoll_event *GetEventsPtr()
    {
        return events_;
    }

    int GetFd(int pos)
    {
        return events_[pos].data.fd;
    }

    std::string GetEvent(int pos)
    {
        switch (events_[pos].events)
        {
        case EPOLLIN:
            return "r";
        case EPOLLOUT:
            return "w";
        case EPOLLIN | EPOLLOUT:
            return "rw";
        }
    }

private:
    struct epoll_event *events_;
};

class Epoller
{
public:
    Epoller() : epfd_(defaultfd)
    {
    }

    void Create()
    {
        int epfd = epoll_create(epollsize);
        if (epfd < 0)
        {
            LogMessage(FATAL, "epoll create failed, errno: %d - strerror: %s\n", errno, strerror(errno));
            exit(EPOLL_CREATE_ERR);
        }
        epfd_ = epfd;
    }

    void Register(int fd, const std::string &estr) // 向epoll模型中注册新的fd
    {
        struct epoll_event event;
        event.events = myevents[estr];
        event.data.fd = fd;
        CtlHelper(EPOLL_CTL_ADD, fd, &event);
    }

    void Modify(int fd, const std::string &estr) // 修改epoll模型中fd的关心事件
    {
        struct epoll_event event;
        event.events = myevents[estr];
        event.data.fd = fd;
        CtlHelper(EPOLL_CTL_MOD, fd, &event);
    }

    void Remove(int fd) // 删除epoll模型中fd
    {
        CtlHelper(EPOLL_CTL_DEL, fd, nullptr);
    }

    int Wait(Events& events, int maxevents, int timeout)
    {
        int readynum = epoll_wait(epfd_, events.GetEventsPtr(), maxevents, timeout);
        if (readynum < 0)
        {
            // 可能epfd无效了, 这是FATAL错误!
            LogMessage(FATAL, "epoll wait failed, errno: %d - strerror: %s\n", errno, strerror(errno));
            exit(EPOLL_WAIT_ERR);
        }

        return readynum;
    }

    ~Epoller()
    {
        if (epfd_ >= 0)
            close(epfd_);
    }

private:
    void CtlHelper(int op, int fd, struct epoll_event *event)
    {
        int ret = epoll_ctl(epfd_, op, fd, event);
        if (ret < 0)
        {
            LogMessage(FATAL, "epoll ctl failed, errno: %d - strerror: %s\n", errno, strerror(errno));
            exit(EPOLL_CTL_ERR);
        }
    }

private:
    int epfd_;
};
