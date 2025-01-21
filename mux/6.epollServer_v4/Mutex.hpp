#pragma once
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>

// 将mutex包装成一个类，对其初始化、销毁、加锁、解锁都有类成员函数完成
class Mutex
{
public:
    Mutex()
    {
        int ret = pthread_mutex_init(&_mutex, nullptr);
        if (ret != 0)
            exit(1);
    }

    void lock()
    {
        int ret = pthread_mutex_lock(&_mutex);
        if (ret != 0)
            exit(2);
    }

    void unlock()
    {
        int ret = pthread_mutex_unlock(&_mutex);
        if (ret != 0)
            exit(3);
    }

    pthread_mutex_t *getmutex()
    {
        return &_mutex;
    }

    ~Mutex()
    {
        int ret = pthread_mutex_destroy(&_mutex);
        if (ret != 0)
            exit(4);
    }

private:
    pthread_mutex_t _mutex;
};

class lockGuard
{
public:
    lockGuard(Mutex* pmutex) : _pmutex(pmutex)
    {
        _pmutex->lock();
    }

    ~lockGuard()
    {
        _pmutex->unlock();
    }

private:
    Mutex* _pmutex;
};