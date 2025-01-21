#pragma once
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "err.hpp"
#include "log.hpp"

class Sock
{
    static const int backlog = 32;

public:
    Sock() : _sockfd(-1) {}
    ~Sock()
    {
        Close();
    }

    void Socket()
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            LogMessage(FATAL, "socket fail\n");
            exit(SOCKET_ERR);
        }
        LogMessage(DEBUG, "socket success: %d\n", sockfd);

        _sockfd = sockfd;

        int optval = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    }

    // server call
    void Bind(const uint16_t &port)
    {
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = INADDR_ANY;

        if (bind(_sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        {
            LogMessage(FATAL, "bind fail: %s\n", strerror(errno));
            exit(BIND_ERR);
        }
        LogMessage(DEBUG, "bind success\n");
    }

    // server call
    void Listen()
    {
        if (listen(_sockfd, backlog) < 0)
        {
            LogMessage(FATAL, "listen fail\n");
            exit(LISTEN_ERR);
        }
        LogMessage(DEBUG, "listen success\n");
    }

    // server call
    int Accept(std::string *cln_ip = nullptr, uint16_t *cln_port = nullptr)
    {
        struct sockaddr_in cln;
        bzero(&cln, sizeof(cln));
        socklen_t len = sizeof(cln);

        int fd = accept(_sockfd, (struct sockaddr *)&cln, &len);
        if (fd < 0)
        {
            LogMessage(INFO, "accept no client\n");
        }
        else
        {
            if (cln_ip)
                *cln_ip = inet_ntoa(cln.sin_addr);
            if (cln_port)
                *cln_port = ntohs(cln.sin_port);
            LogMessage(DEBUG, "new client accepted\n");
        }
        return fd;
    }

    // client call
    int Connect(const std::string &svr_ip, const uint16_t &svr_port)
    {
        struct sockaddr_in svr;
        svr.sin_family = AF_INET;
        svr.sin_port = htons(svr_port);
        svr.sin_addr.s_addr = inet_addr(svr_ip.c_str());

        int ret = connect(_sockfd, (struct sockaddr *)&svr, sizeof(svr));
        if (ret < 0)
            LogMessage(WARNING, "connect fail: %s\n", strerror(errno));
        else
            LogMessage(DEBUG, "connect success\n");

        return ret;
    }

    void Close()
    {
        if (_sockfd >= 0)
            close(_sockfd);
    }

    int GetSockfd() const
    {
        return _sockfd;
    }

private:
    int _sockfd;
};
