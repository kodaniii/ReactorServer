#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

ThreadPool::ThreadPool(EventLoop* mainLoop, int count)
{
    m_index = 0;
    m_isStart = false;
    m_mainLoop = mainLoop;
    m_threadNum = count;
    m_SubThreads.clear();
}

ThreadPool::~ThreadPool()
{
    for (auto item : m_SubThreads)
    {
        delete item;
    }
}

void ThreadPool::run()
{
    assert(!m_isStart);
    //只允许主线程分配子线程, 防止子线程异常异常调用run()
    if (m_mainLoop->getThreadID() != this_thread::get_id())
    {
        exit(0);
    }
    m_isStart = true;
    if (m_threadNum > 0)
    {
        for (int i = 0; i < m_threadNum; ++i)
        {
            SubThread* subThread = new SubThread(i);
            subThread->run();
            m_SubThreads.push_back(subThread);
        }
    }
}

EventLoop* ThreadPool::getWorkerEventLoop()
{
    assert(m_isStart);
    if (m_mainLoop->getThreadID() != this_thread::get_id())
    {
        exit(0);
    }
    // 从线程池中找一个子线程, 然后取出里边的反应堆实例
    EventLoop* evLoop = m_mainLoop;
    if (m_threadNum > 0)
    {
        evLoop = m_SubThreads[m_index]->getEventLoop();
        m_index = ++m_index % m_threadNum;
    }
    return evLoop;
}
