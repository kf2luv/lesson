#pragma once

#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <functional>
#include <unistd.h>
#include <cstdlib>

using namespace std;

class Thread
{
public:
    typedef void *(*prun)(void *);
    typedef enum
    {
        NEW,
        RUNNING,
        EXITED
    } status;

public:
    Thread() : _tid(0), _index(0), _func(nullptr), _arg(nullptr), _stat(NEW)
    {
    }

    // 创建线程时，传入线程编号、线程函数、传入线程函数的参数
    Thread(int index, prun func, void *arg) : _tid(0), _index(index), _func(func), _arg(arg), _stat(NEW)
    {
        char name[64] = {0};
        snprintf(name, sizeof(name), "thread-%d", index);
        _name = name;
    }

    ~Thread()
    {
    }

    pthread_t getTid()
    {
        return _tid;
    }

    string getName()
    {
        return _name;
    }

    void operator()()
    {
        _func(_arg);
    }

    static void *runHelper(void *args)
    {
        Thread *tp = static_cast<Thread *>(args);
        (*tp)();
        return nullptr;
    }

    void run()
    {
        int ret = pthread_create(&_tid, nullptr, runHelper, this);
        if (ret != 0)
            exit(1);
        _stat = RUNNING;
    }

    void join()
    {
        int ret = pthread_join(_tid, nullptr);
        if (ret != 0)
            exit(1);
        _stat = EXITED;
    }

private:
    pthread_t _tid;
    string _name; // 线程名称
    int _index;   // 线程编号
    status _stat; // 线程状态

    prun _func;
    void *_arg;
};