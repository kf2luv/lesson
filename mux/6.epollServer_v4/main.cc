#include "reactor_server.hpp"
#include <memory>

Response calculator(const Request &req)
{
    Response resp;
    switch (req._opt)
    {
    case '+':
        resp._ret = req._x + req._y;
        break;
    case '-':
        resp._ret = req._x - req._y;
        break;
    case '*':
        resp._ret = req._x * req._y;
        break;
    case '/':
        if (req._y == 0)
            resp._code = 1;
        else
            resp._ret = req._x / req._y;
        break;
    case '%':
        if (req._y == 0)
            resp._code = 2;
        else
            resp._ret = req._x % req._y;
        break;
    default:
        resp._code = 3;
    }
    return resp;
}

int main()
{
    std::unique_ptr<ReactorServer> svr(new ReactorServer(calculator));
    svr->Init();
    svr->Start();

    return 0;
}