#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>

const size_t svr_port = 8080;

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket failed: ");
        exit(-1);
    }

    struct sockaddr_in svr_addr;
    bzero(&svr_addr, sizeof(svr_addr));
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_port = htons(svr_port);
    svr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) == -1)
    {
        perror("connect failed: ");
        exit(-1);
    }

    // connect success
    // todo

    bool is_conn = true;
    while (is_conn)
    {
        // communicate with client
    }
}

