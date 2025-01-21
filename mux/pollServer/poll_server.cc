#include "poll_server.hpp"
#include <memory>

int main()
{
    std::unique_ptr<PollServer> svr(new PollServer);
    svr->Init();
    svr->Start();

    return 0;
}