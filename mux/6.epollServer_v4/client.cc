#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include "mysocket.hpp"
#include "protocol_netcal.hpp"
#include "err.hpp"

void Usage()
{
    std::cout << "Please enter the correct format: "
              << "./client [server's ip] [server's port]" << std::endl;
}

using namespace protocol_ns_json;

int Enter(Request &req)
{
    std::string str;
    std::getline(std::cin, str);
    if (str == "quit")
        return -1;
    // 1+2
    int i = 0;
    while (i < str.size() && isdigit(str[i]))
    {
        i++;
    }
    if (i == str.size())
        return 0;
    // 除opt外后面不能再有非数字符号
    int j = i + 1;
    if (j == str.size())
        return 0;
    while (j < str.size())
    {
        if (!isdigit(str[j]))
            return 0;
        j++;
    }
    // str[i]必须是"+-*/%"
    const char *opts = "+-*/%";
    bool flag = 0;
    while (opts)
    {
        if (str[i] == *opts)
        {
            flag = true;
            break;
        }
        ++opts;
    }
    if (!flag)
        return 0;

    req._opt = str[i];
    req._x = std::stoi(str.substr(0, i));
    req._y = std::stoi(str.substr(i + 1, j - i - 1));
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        Usage();
        exit(USAGE_ERR);
    }

    Sock connectsock;
    connectsock.Socket();
    std::string svr_ip(argv[1]);
    uint16_t svr_port = atoi(argv[2]);

    if (connectsock.Connect(svr_ip, svr_port) < 0)
        exit(CONNECT_ERR);

    while (true)
    {
        // 1.用户输入计算任务
        Request req;
        std::cout << "Enter Calculate Task:> ";
        int flag = Enter(req);
        if (flag == -1)
        {
            std::cout << "calculator quit" << std::endl;
            break;
        }
        if (flag == 0)
        {
            std::cout << "非法计算式，请重新输入" << std::endl;
            continue;
        }

        // 2.序列化计算请求
        std::string reqStr;
        req.Serialize(&reqStr);

        // 3.添加报头
        AddHeader(reqStr);

        // 4.发送str到服务器
        ssize_t n = send(connectsock.GetSockfd(), reqStr.c_str(), reqStr.size(), 0);
        if (n < 0)
        {
            exit(SEND_ERR);
        }

        std::cout << "发送成功: " << reqStr.c_str() << std::endl;

        // 5.接收服务器发回的响应
        std::string inbuffer;
        std::string response;

        while (true)
        {
            char buffer[1024] = {0};
            ssize_t n = recv(connectsock.GetSockfd(), buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                connectsock.Close();
                return RECV_ERR;
            }
            else if (n == 0)
            {
                LogMessage(DEBUG, "与服务器断开连接了...\n");
                connectsock.Close();
                return 0;
            }
            else
            {
                buffer[n] = 0;
                inbuffer += buffer;
                int len = Parse(inbuffer, &response);
                if (len == 0)
                    continue;
                else
                {
                    // 读取到一个完整的响应package
                    std::cout << "接收成功: " << response.c_str() << std::endl;
                    // 6.解开报头
                    RemoveHeader(response, len);
                    break;
                }
            }
        }

        // 7.反序列化响应
        Response resp;
        resp.Deserialize(response);

        // 8.show result
        std::cout << resp._ret << " [code: " << resp._code << "]" << std::endl;
    }

    return 0;
}
