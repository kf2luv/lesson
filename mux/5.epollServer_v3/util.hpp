#pragma once

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "log.hpp"

namespace util
{
    bool SetNonBlock(int fd)
    {
        int oldflags = fcntl(fd, F_GETFL);
        if (oldflags < 0)
        {
            return false;
        }
        int n = fcntl(fd, F_SETFL, oldflags | O_NONBLOCK);
        if (n < 0)
        {
            return false;
        }
        return true;
    }
};