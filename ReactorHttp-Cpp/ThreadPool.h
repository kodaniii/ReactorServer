#pragma once
#include "EventLoop.h"
#include <stdbool.h>
#include "SubThread.h"
#include <vector>
using namespace std;

// 定义线程池
class ThreadPool
{
public:
    ThreadPool(EventLoop* mainLoop, int count);
    ~ThreadPool();
    // 启动线程池
    void run();
    // 取出线程池中的某个子线程的反应堆实例
    EventLoop* getWorkerEventLoop();
private:
    // 主线程的反应堆模型
    EventLoop* m_mainLoop;
    bool m_isStart;
    int m_threadNum;    //子线程数量
    vector<SubThread*> m_SubThreads;  //子线程工作线程
    int m_index;
};

