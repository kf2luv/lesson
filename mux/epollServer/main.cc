#include "epoll_server.hpp"
#include <memory>

int main()
{
    std::unique_ptr<EpollServer> svr(new EpollServer);
    svr->Init();
    svr->Start();

    return 0;
}