#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <thread>

const size_t svr_port = 8080;

int main()
{
    // 1.create listen socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        perror("socket failed: ");
        exit(-1);
    }
    std::cout << "socket successed" << std::endl;

    // 2.bind
    struct sockaddr_in svr_addr;
    bzero(&svr_addr, sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_port = htons(svr_port);
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) == -1)
    {
        perror("bind failed: ");
        exit(-2);
    }
    std::cout << "bind successed" << std::endl;

    // 3.start to listen
    if (listen(listen_fd, 128) == -1)
    {
        perror("listen failed: ");
        exit(-3);
    }
    std::cout << "listen successed" << std::endl;

    // 4.try to accpet
    struct sockaddr cli_addr;
    socklen_t addrlen = 0;

    while (true)
    {
        // 5.accept a new connection
        int fd = accept(listen_fd, &cli_addr, &addrlen);
        std::cout << "accept successed: fd = " << fd << std::endl;
        if (fd == -1)
        {
            perror("accept failed: ");
            exit(-4);
        }

        // 6.accepted, start the business
        std::thread worker([](int fd)
                           {
            bool is_conn = true;
            while(is_conn){
                //communicate with client
            } }, fd);
        worker.detach();
    }

    return 0;
}
