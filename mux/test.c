#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>

void setNonBlock(int fd)
{
    // 读取fd的状态
    int fl = fcntl(fd, F_GETFL);
    if (fl < 0)
    {
        perror("fcntl error");
        return;
    }
    // 设置fd的状态为非阻塞
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// 非阻塞读取标准输入
int main()
{
    setNonBlock(0);
    char readBuf[1024];
    bzero(readBuf, sizeof(readBuf));

    //轮询读取数据
    while (1)
    {
        ssize_t bytes = read(0, readBuf, sizeof(readBuf) - 1);
        if (bytes < 0)
        {
            // 数据未就绪
            perror("read error");
            sleep(1);
            continue;
        }
        printf("your inputs: %s\n", readBuf);
        break;
    }

    return 0;
}
