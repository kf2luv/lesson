#pragma once
#include <iostream>

#include <unistd.h>
#include <sys/select.h>

#include "mysocket.hpp"

static const uint16_t default_port = 8080;

class SelectServer
{
public:
    SelectServer(uint16_t port = default_port) : port_(port)
    {
    }

    void Init()
    {
        listensock_.Socket();
        listensock_.Bind(port_);
        listensock_.Listen();
    }

    void Start()
    {
        // 扫描当前进程维护的文件fd (从listensock开始)
        // 等待读事件就绪 (检测有没有读事件就绪的，如果有就处理，没有则继续)


        while(true)
        {
            // int n = select()
        }


    }


private:
    uint16_t port_;
    Sock listensock_;
};