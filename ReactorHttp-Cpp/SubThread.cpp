#include "SubThread.h"
#include <stdio.h>

SubThread::SubThread(int index)
{
    m_evLoop = nullptr;
    m_thread = nullptr;
    m_threadID = thread::id();
    m_name =  string("SubThread_") + to_string(index);
}

SubThread::~SubThread()
{
    if (m_thread != nullptr)
    {
        delete m_thread;
    }
}

void SubThread::running()
{
    m_mutex.lock();
    m_evLoop = new EventLoop(m_name);
    m_mutex.unlock();
    
    m_cond.notify_one();
    m_evLoop->run();
}

void SubThread::run()
{
    // 创建子线程
    m_thread = new thread(&SubThread::running, this);
    /* 阻塞主线程, 让当前函数不会直接结束*/
    //m_evLoop共享资源, 加互斥锁
    unique_lock<mutex> locker(m_mutex);
    while (m_evLoop == nullptr)
    {
        m_cond.wait(locker);
    }
}
