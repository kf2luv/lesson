#pragma once
#include <iostream>
#include <string>
#include <functional>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <jsoncpp/json/json.h>
#include "log.hpp"
#include "err.hpp"

#define SEP " "
#define SEP_LEN strlen(SEP)
#define HEADER_SEP "\r\n"
#define HEADER_SEP_LEN strlen(HEADER_SEP)

// namespace protocol_ns
// {
//     bool AddHeader(std::string &str)
//     {
//         if (str.empty())
//             return false;
//         std::string lenStr = std::to_string(str.size());
//         str = lenStr + HEADER_SEP + str;
//         return true;
//     }

//     std::string RemoveHeader(std::string package, size_t bodyLen) // package: 包裹字符串 bodyLen: 有效载荷长度
//     {
//         return package.substr(package.size() - bodyLen, bodyLen);
//     }

//     // 读取套接字失败返回-1
//     // 未能获取完整package返回0
//     // 成功获取完整package返回有效载荷长度len
//     int ReadPackage(const int &sock, std::string &readBuf, std::string *package)
//     {
//         // 1.一个完整的package可能需要多次读取才能成功（对端未发送完毕、缓冲区容量不足等问题导致）
//         char buf[1024] = {0};
//         ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
//         if (n <= 0)
//         {
//             LogMessage(FATAL, "recv fail\n");
//             return -1;
//         }
//         buf[n] = '\0';
//         readBuf += buf;
//         // std::cout << "当前readBuf" << readBuf << std::endl;

//         // 2.找到报头——即有效载荷长度字符串
//         int lenEnd = readBuf.find(HEADER_SEP);
//         if (lenEnd == std::string::npos)
//             return 0;
//         std::string lenStr = readBuf.substr(0, lenEnd);
//         int len = std::stoi(lenStr);

//         // std::cout << "有效载荷长度：" << len << std::endl;

//         // 3.确定package整体长度
//         int packageLen = len + lenStr.size() + HEADER_SEP_LEN;
//         if (readBuf.size() < packageLen) // 缓冲区长度不足目标package长度
//             return 0;

//         // std::cout << "package长度：" << packageLen << std::endl;

//         // 4.输出并擦除package,
//         *package = readBuf.substr(0, packageLen);
//         readBuf.erase(0, packageLen);
//         return len;
//     }

//     // 客户端的请求
//     // str: _x + _y
//     class Request
//     {
//     public:
//         Request(int x = 0, char opt = 0, int y = 0)
//             : _x(x), _opt(opt), _y(y)
//         {
//         }

//         // "7\r\n10 + 20"
//         // struct->str
//         bool Serialize(std::string *outStr)
//         {
//             *outStr = std::to_string(_x) + SEP + _opt + SEP + std::to_string(_y);
//             return true;
//         }

//         // str->struct
//         bool Deserialize(const std::string &inStr)
//         {
//             int pos = inStr.find(SEP);
//             if (pos == std::string::npos)
//                 return false;

//             std::string xStr = inStr.substr(0, pos);
//             _opt = inStr[++pos];
//             pos += SEP_LEN + 1;
//             std::string yStr = inStr.substr(pos, inStr.size() - pos);

//             _x = std::stoi(xStr);
//             _y = std::stoi(yStr);
//             return true;
//         }

//     public:
//         int _x;
//         int _y;
//         char _opt;
//     };

//     // 服务器的响应
//     // str: _ret _code
//     class Response
//     {
//     public:
//         bool Serialize(std::string *outStr)
//         {
//             if (outStr == nullptr)
//                 return false;
//             *outStr = std::to_string(_ret) + SEP + std::to_string(_code);
//             return true;
//         }

//         bool Deserialize(const std::string &inStr)
//         {
//             int sepPos = inStr.find(SEP);
//             if (sepPos == std::string::npos)
//                 return false;
//             std::string retStr = inStr.substr(0, sepPos);
//             std::string codeStr = inStr.substr(sepPos + 1, inStr.size());
//             _ret = std::stoi(retStr);
//             _code = std::stoi(codeStr);
//             return true;
//         }

//     public:
//         int _ret;
//         int _code = 0; // 1/2/3表示不同的错误码
//     };
// };

namespace protocol_ns_json
{
    bool AddHeader(std::string &str)
    {
        if (str.empty())
            return false;
        std::string lenStr = std::to_string(str.size());
        str = lenStr + HEADER_SEP + str;
        return true;
    }

    bool RemoveHeader(std::string &package, int bodyLen) // package: 包裹字符串 bodyLen: 有效载荷长度
    {
        if (package.empty())
            return false;
        package = package.substr(package.size() - bodyLen, bodyLen);
        return true;
    }

    // 分析readBuf是否有完整的报文, 如果有, 拷贝到package中, 并返回该报文的长度len
    // 未能获取完整package 返回0
    // 成功获取完整package 返回有效载荷长度len
    int Parse(std::string &readBuf, std::string *package)
    {
        // 1.找到报头——即有效载荷长度字符串
        int lenEnd = readBuf.find(HEADER_SEP);
        if (lenEnd == std::string::npos)
            return 0;
        std::string lenStr = readBuf.substr(0, lenEnd);
        int len = std::stoi(lenStr);

        // 2.确定package整体长度
        int packageLen = len + lenStr.size() + HEADER_SEP_LEN;
        if (readBuf.size() < packageLen) // 缓冲区长度不足目标package长度
            return 0;

        // 3.输出并擦除package
        *package = readBuf.substr(0, packageLen);
        readBuf.erase(0, packageLen);
        return len;
    }

    // // 读取套接字失败返回-1
    // int ReadPackage(const int &sock, std::string &readBuf, std::string *package)
    // {
    //     // 1.一个完整的package可能需要多次读取才能成功（对端未发送完毕、缓冲区容量不足等问题导致）
    //     char buf[1024] = {0};
    //     ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    //     if (n <= 0)
    //     {
    //         LogMessage(FATAL, "recv fail\n");
    //         return -1;
    //     }
    //     buf[n] = '\0';
    //     readBuf += buf;
    //     // std::cout << "当前readBuf" << readBuf << std::endl;

    //     return Parse(readBuf, package);
    // }

    // 客户端的请求
    // str: _x + _y
    class Request
    {
    public:
        Request(int x = 0, char opt = 0, int y = 0)
            : _x(x), _opt(opt), _y(y)
        {
        }

        // "7\r\n10 + 20"
        // struct->str
        bool Serialize(std::string *outStr)
        {
            if (outStr == nullptr)
                return false;

            Json::Value root;
            root["x"] = _x;
            root["opt"] = _opt;
            root["y"] = _y;

            Json::StyledWriter writer;
            *outStr = writer.write(root);
            return true;
        }

        // str->struct
        bool Deserialize(const std::string &inStr)
        {
            Json::Value root;
            Json::Reader reader;
            // bool parse(const std::string& document, Value& root, bool collectComments = true);
            reader.parse(inStr, root);
            _x = root["x"].asInt();
            _opt = root["opt"].asInt();
            _y = root["y"].asInt();
            return true;
        }

    public:
        int _x;
        int _y;
        char _opt;
    };

    // 服务器的响应
    // str: _ret _code
    class Response
    {
    public:
        bool Serialize(std::string *outStr)
        {
            if (outStr == nullptr)
                return false;

            Json::Value root;
            root["ret"] = _ret;
            root["code"] = _code;

            Json::StyledWriter writer;
            *outStr = writer.write(root);
            return true;
        }

        bool Deserialize(const std::string &inStr)
        {
            Json::Value root;
            Json::Reader reader;
            reader.parse(inStr, root);
            _ret = root["ret"].asInt();
            _code = root["code"].asInt();
            return true;
        }

    public:
        int _ret = 0;
        int _code = 0; // 1/2/3表示不同的错误码
    };

    using service_t = std::function<Response(const Request &)>;

    // 传入请求序列reqstr和有效载荷长度payload_len, 还有业务处理方法, 返回响应序列respstr
    std::string HandleRequest2Response(std::string reqstr, int payload_len, service_t &service)
    {
        // 1.req去报头
        RemoveHeader(reqstr, payload_len);
        // 2.req反序列化
        Request req;
        req.Deserialize(reqstr);
        // 3.业务处理->得到响应
        Response resp = service(req);
        // 4.resp序列化
        std::string respstr;
        resp.Serialize(&respstr);
        // 5.resp加报头
        AddHeader(respstr);

        return respstr;
    }
};