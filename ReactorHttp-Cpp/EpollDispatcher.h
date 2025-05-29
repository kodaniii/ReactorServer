#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include <string>
#include <sys/epoll.h>
using namespace std;

class EpollDispatcher : public Dispatcher
{
public:
    EpollDispatcher(EventLoop* evloop);
    ~EpollDispatcher();
    // 添加
    int add() override;
    // 删除
    int remove() override;
    // 修改监听事件
    int modify() override;
    // 事件监听
    int dispatch(int timeout = 2) override; // 单位: s

private:
    //将当前channel->fd在epoll中加入/删除/修改
    int epollCtl(int op);

private:
    int m_epfd;
    struct epoll_event* m_events;   //每轮监控到的就绪文件描述符
    const int m_maxNode = 520;      //每轮监控最大数量
};