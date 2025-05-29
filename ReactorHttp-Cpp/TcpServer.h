#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

class TcpServer
{
public:
    TcpServer(unsigned short port, int threadNum);
    // 初始化监听
    void setListen();
    // 启动服务器
    void run();
    static int acceptConnection(void* arg);

private:
    int m_threadNum;    //子线程数量
    EventLoop* m_mainLoop;  //主线程主反应堆
    ThreadPool* m_threadPool;   //子线程子反应堆
    int m_lfd;  //监听fd
    unsigned short m_port;
};

