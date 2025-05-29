#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SelectDispatcher.h"
#include "PollDispatcher.h"
#include "EpollDispatcher.h"

//调用EventLoop("")
EventLoop::EventLoop() : EventLoop(string())
{
}

EventLoop::EventLoop(const string threadName)
{
    m_isStop = true;    // 默认没有启动
    m_threadID = this_thread::get_id();
    /*通过EventLoop("")调用的threadName是主线程*/
    m_threadName = threadName == string() ? "MainThread" : threadName;

    m_dispatcher = new EpollDispatcher(this);
    // map
    m_channelMap.clear();

    //int socketpair(int domain, int type, int protocol, int sv[2]);
    //AF_UNIX用于本地通信，AF_INET用于网络通信（绑定IP+端口）
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
#if 0
    // 指定规则: evLoop->socketPair[0] 发送数据, evLoop->socketPair[1] 接收数据
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
        readLocalMessage, nullptr, nullptr, this);
#else
    //新建一个channel, 封装了socketpair[1]并设置write事件监听
    auto func_readMessage = bind(&EventLoop::readMessage, this);
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
        func_readMessage, nullptr, nullptr, this);
#endif
    // 添加channel到任务队列
    addChannelToQ(channel, ElemType::ADD);
}

EventLoop::~EventLoop()
{
}

int EventLoop::run()
{
    m_isStop = false;
    //异常判断：线程ID和m_threadID是否相等
    if (m_threadID != this_thread::get_id())
    {
        return -1;
    }
    // 循环进行事件处理
    while (!m_isStop)
    {
        //对于epoll，调用epoll_wait，并调用EventLoop::eventActive触发回调函数
        m_dispatcher->dispatch(2);    // 超时时长2s
        processChannelQ();
    }
    return 0;
}

int EventLoop::eventActive(int fd, int event)
{
    if (fd < 0)
    {
        return -1;
    }
    //对于epoll，根据epoll的返回的就绪fd，从map中取出channel
    Channel* channel = m_channelMap[fd];
    assert(channel->getSocket() == fd);
    if (event & (int)FDEvent::ReadEvent && channel->readCallback)
    {
        channel->readCallback(const_cast<void*>(channel->getArg()));
    }
    if (event & (int)FDEvent::WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(const_cast<void*>(channel->getArg()));
    }
    return 0;
}

int EventLoop::addChannelToQ(Channel* channel, ElemType type)
{
    // 加锁, 保护共享资源(任务队列)
    m_mutex.lock();

    // 创建新Channel节点
    ChannelElement* node = new ChannelElement();
    node->channel = channel;
    node->type = type;
    m_channelQ.push(node);

    m_mutex.unlock();
    // 只能由子线程处理任务队列, 调用processChannelQ()
    /*
    *   1. 对于任务队列中添加节点: 可能是当前子线程也可能是主线程
    *       1). 修改fd的事件, 当前子线程发起, 当前子线程处理
    *       2). 添加新的fd, 添加任务节点的操作是由主线程发起的
    *   2. 不能让主线程处理任务队列, 需要由当前的子线程取处理
    */
    if (m_threadID == this_thread::get_id())
    {
        // 子线程处理任务队列
        processChannelQ();
    }
    else
    {
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1. 子线程在工作 2. 子线程被select, poll, epoll函数阻塞(子反应堆调用dispatcher时)
        /*
            为了避免子线程被阻塞时不能处理任务队列, 通过socketpair建立一对进程内通信的文件描述符(相互连通), 
            子反应堆监听读端, 主线程写后, 子线程可读就绪, 就不会由于没有就绪文件描述符而阻塞了
        */
        channelWakeup();
    }
    return 0;
}

int EventLoop::processChannelQ()
{
    while (!m_channelQ.empty())
    {
        m_mutex.lock();
        ChannelElement* node = m_channelQ.front();
        m_channelQ.pop();
        m_mutex.unlock();

        Channel* channel = node->channel;
        if (node->type == ElemType::ADD)
        {
            // 添加
            add(channel);
        }
        else if (node->type == ElemType::DELETE)
        {
            // 删除
            remove(channel);
        }
        else if (node->type == ElemType::MODIFY)
        {
            // 修改
            modify(channel);
        }
        delete node;
    }
    return 0;
}

int EventLoop::add(Channel* channel)
{
    int fd = channel->getSocket();
    // 找到fd对应的数组元素位置, 并存储
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        m_channelMap.insert(make_pair(fd, channel));
        m_dispatcher->setChannel(channel);
        int ret = m_dispatcher->add();
        return ret;
    }
    return -1;
}

int EventLoop::remove(Channel* channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->remove();
    return ret;
}

int EventLoop::modify(Channel* channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->modify();
    return ret;
}

int EventLoop::freeChannel(Channel* channel)
{
    // 删除 channel 和 fd 的对应关系
    auto it = m_channelMap.find(channel->getSocket());
    if (it != m_channelMap.end())
    {
        m_channelMap.erase(it);
        close(channel->getSocket());
        delete channel;
    }
    return 0;
}

void EventLoop::channelWakeup()
{
    const char* msg = "wakeup...";
    write(m_socketPair[0], msg, strlen(msg));
}

int EventLoop::readMessage()
{
    char buf[256];
    read(m_socketPair[1], buf, sizeof(buf));
    return 0;
}

int EventLoop::readLocalMessage(void* arg)
{
    EventLoop* evLoop = static_cast<EventLoop*>(arg);
    char buf[256];
    read(evLoop->m_socketPair[1], buf, sizeof(buf));
    return 0;
}