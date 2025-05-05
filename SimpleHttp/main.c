#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "Server.h"

int main(int argc, char* argv[])
{
#if 0
    if (argc < 3)
    {
        printf("usage: ./server.out port path\n");
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    // 切换服务器的工作路径
    chdir(argv[2]);
#else
    unsigned short port = 8087;
    chdir("/root");
#endif
    // 初始化用于监听的套接字
    int lfd = initListenFd(port);
    // 启动服务器程序
    epollRun(lfd);

    return 0;
}