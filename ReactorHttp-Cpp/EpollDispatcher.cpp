#include "Dispatcher.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "EpollDispatcher.h"
#include "Log.h"
#include <fcntl.h>

EpollDispatcher::EpollDispatcher(EventLoop* evloop) : Dispatcher(evloop)
{
    m_epfd = epoll_create(10);
    if (m_epfd == -1)
    {
        perror("epoll_create");
        exit(0);
    }
    m_events = new struct epoll_event[m_maxNode];
    m_name = "Epoll";
}

EpollDispatcher::~EpollDispatcher()
{
    close(m_epfd);
    delete[] m_events;
}

int EpollDispatcher::add()
{
    int ret = epollCtl(EPOLL_CTL_ADD);
    if (ret == -1)
    {
        perror("epoll_ctl add");
        exit(0);
    }
    return ret;
}

int EpollDispatcher::remove()
{
    int ret = epollCtl(EPOLL_CTL_DEL);
    if (ret == -1)
    {
        perror("epoll_ctl delete");
        exit(0);
    }
    // 通过 channel 释放对应的 TcpConnection 资源
    m_channel->destroyCallback(const_cast<void*>(m_channel->getArg()));

    return ret;
}

int EpollDispatcher::modify()
{
    int ret = epollCtl(EPOLL_CTL_MOD);
    if (ret == -1)
    {
        perror("epoll_ctl modify");
        exit(0);
    }
    return ret;
}

int EpollDispatcher::dispatch(int timeout)
{
    int count = epoll_wait(m_epfd, m_events, m_maxNode, timeout * 1000);
    for (int i = 0; i < count; ++i)
    {
        int events = m_events[i].events;
        int fd = m_events[i].data.fd;
        //EPOLLERR：对方断开连接
        //EPOLLHUP：我方发送数据，发现对方断开连接
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            /*TODO：目前实现不了，EventLoop还没做好 */
            // 对方断开了连接, 删除 fd
            // epollRemove(Channel, evLoop);
            continue;
        }
        if (events & EPOLLIN)   //events is not EPOLLET!!!!
        {
            //if(!(events & EPOLLET)) {
            //    Debug("events is not EPOLLET!!!!\n");
            //}
            //激活fd封装的channel对应EPOLLIN事件的回调函数
            m_evLoop->eventActive(fd, (int)FDEvent::ReadEvent);
        }
        if (events & EPOLLOUT)  //events is not EPOLLET!!!!
        {
            //if(!(events & EPOLLET)) {
            //    Debug("events is not EPOLLET!!!!\n");
            //}
            m_evLoop->eventActive(fd, (int)FDEvent::WriteEvent);
        }
    }
    return 0;
}

int EpollDispatcher::epollCtl(int op)
{
    printf("EpollDispatcher::epollCtl\n");
    struct epoll_event ev;
    ev.data.fd = m_channel->getSocket();
    #define ET_MODE 0
    if(ET_MODE && op == EPOLL_CTL_ADD){
        // 2. 设置非阻塞
        int flag = fcntl(ev.data.fd, F_GETFL);
        flag |= O_NONBLOCK;     //epoll ET必须设置为非阻塞模式
        fcntl(ev.data.fd, F_SETFL, flag);
    }

    int events = 0;
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent)
    {
        events |= EPOLLIN;
        if(ET_MODE) events |= EPOLLET;
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent)
    {
        events |= EPOLLOUT;
        if(ET_MODE) events |= EPOLLET;
    }
    ev.events = events;
    int ret = epoll_ctl(m_epfd, op, m_channel->getSocket(), &ev);
    return ret;
}
