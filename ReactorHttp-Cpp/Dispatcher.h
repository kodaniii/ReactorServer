#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include <string>
using namespace std;
class EventLoop;
class Dispatcher
{
public:
    //初始化监听fd和epoll_events
    Dispatcher(EventLoop* evloop);
    //free监听fd和epoll_events
    virtual ~Dispatcher();
    // 添加
    virtual int add() = 0;
    // 删除
    virtual int remove() = 0;
    // 修改监听事件
    virtual int modify() = 0;
    // 事件监听
    virtual int dispatch(int timeout = 2) = 0; // 单位: s
    inline void setChannel(Channel* channel)
    {
        m_channel = channel;
    }

protected:
    string m_name = string();   //select/poll/epoll
    Channel* m_channel;
    EventLoop* m_evLoop;
};