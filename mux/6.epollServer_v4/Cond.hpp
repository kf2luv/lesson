#pragma once

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>
#include "Mutex.hpp"

class Cond
{
public:
    Cond()
    {
        int ret = pthread_cond_init(&cond_, nullptr);
        if (ret < 0)
            exit(1);
    }
    ~Cond()
    {
        int ret = pthread_cond_destroy(&cond_);
        if (ret < 0)
            exit(2);
    }

    void Wakeup()
    {
        pthread_cond_signal(&cond_);
    }

    bool Wait(Mutex &mutex, int sec) // sec->等待时间(秒)
    {
        // sec == -1 ：阻塞等待
        if (sec == -1)
        {
            pthread_cond_wait(&cond_, mutex.getmutex());
            return true;
        }
        // sec >= 0：非阻塞等待
        else if (sec >= 0)
        {
            struct timespec wait_time;
            clock_gettime(CLOCK_REALTIME, &wait_time);
            wait_time.tv_sec += sec; // 设置等待超时为5秒
            int result = pthread_cond_timedwait(&cond_, mutex.getmutex(), &wait_time);
            return result == 0;
        }
        else
        {
            LogMessage(DEBUG, "非法等待时间: %d\n", sec);
            return false;
        }
    }

private:
    pthread_cond_t cond_;
};
