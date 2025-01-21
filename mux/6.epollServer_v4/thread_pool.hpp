#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <pthread.h>
#include "Thread.hpp"
#include "Mutex.hpp"
#include "reactor.hpp"

static const int max_cap = 3;
static const int default_threadnum = 5;

// 使用说明:
// 1.要自己封装任务类型Task, Task必须包含operator(), 这是该Task的执行函数
// 2.线程池会自己启动, 用户调用时直接使用get_instance, 并传入想要的工作线程个数即可

template <class Task>
class ThreadPool
{
public:
    // _tp也是临界资源, 要保护起来
    static ThreadPool<Task> *get_instance(const int &threadnum = default_threadnum)
    {
        if (_tp == nullptr)
        // 临界区
        {
            lockGuard lg(&_tp_mutex);

            // 只有第一次访问单例时会进入
            // 那么绝大多数情况都是不进入的
            // 所以每次都要申请锁再判断是否进入区块, 效率低
            // 双判断, 解决问题

            if (_tp == nullptr)
            {
                // 第一次访问单例时创建
                _tp = new ThreadPool<Task>(threadnum);
                // 启动所有线程
                _tp->start();
            }
        }

        return _tp;
    }

    ~ThreadPool()
    {
        // 回收线程
        for (auto &thr : _threads)
        {
            thr.join();
        }
        // delete

        pthread_cond_destroy(&_cond);
    }

    void pushTask(const Task &in)
    {
        lockGuard lg(&_mutex);

        _tasks.push(in);
        pthread_cond_signal(&_cond);
    }

private:
    // 禁止用户构造、拷贝、赋值
    ThreadPool(const int &cap) : _threads(cap), _cap(cap)
    {               
        // 创建线程, 所有线程处于等待任务的状态
        for (int i = 0; i < _cap; i++)
        {
            _threads[i] = Thread(i + 1, threadRoutine, this);
        }

        pthread_cond_init(&_cond, nullptr);
    }

    ThreadPool(const ThreadPool<Task> &tp) = delete;
    ThreadPool<Task> &operator=(const ThreadPool<Task> &tp) = delete;

    // 外部无法访问threadRoutine和popTask, 设为私有
    // tips:
    // 非静态公有成员函数->可调用->静态私有成员函数
    // 静态私有(公有不行)成员函数->可通过传入对象实体调用->非静态私有成员函数 ---> tp->popTask()
    // start(线程在类内创建, 所以可以调用类的私有成员函数) -> threadRoutine -> popTask

    static void *threadRoutine(void *args)
    {
        ThreadPool *tp = static_cast<ThreadPool *>(args);

        while (true)
        {
            // 获取任务, 任务队列如果为空，需要等待
            Task t = tp->popTask();

            // 处理任务(线程池应该处理短时任务，有限的线程干无限的事，线程干完一个任务就可以处理下一个任务)
            // 即：t()不能是循环任务
            t();
        }

        return nullptr;
    }

    Task popTask()
    {
        Task out;

        // 临界区
        {
            lockGuard lg(&_mutex);

            while (_tasks.empty())
            {
                pthread_cond_wait(&_cond, _mutex.getmutex());
            }
            out = _tasks.front();
            _tasks.pop();
        }

        return out;
    }
    // 启动线程池, 所有线程开始运行
    void start()
    {
        for (auto &thr : _threads)
        {
            thr.run();
        }
    }

private:
    std::queue<Task> _tasks; // 任务队列大小无限制，采用stl中的自动扩容
    std::vector<Thread> _threads;

    // 消费线程访问任务队列的锁和条件变量
    Mutex _mutex;
    pthread_cond_t _cond;

    int _cap; // 线程池的最大容量

    static ThreadPool<Task> *_tp;
    static Mutex _tp_mutex;
};

// 静态成员的初始化

template <class Task>
ThreadPool<Task> *ThreadPool<Task>::_tp = nullptr;

template <class Task>
Mutex ThreadPool<Task>::_tp_mutex;
