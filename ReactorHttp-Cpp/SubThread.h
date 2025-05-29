#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EventLoop.h"
using namespace std;

// 定义子线程对应的结构体
class SubThread
{
public:
    SubThread(int index);
    ~SubThread();
    // 启动线程
    void run();
    inline EventLoop* getEventLoop()
    {
        return m_evLoop;
    }

private:
    // 子线程的回调函数
    void running();

private:
    EventLoop* m_evLoop;   // 反应堆模型
    thread* m_thread;   // 保存线程的实例
    thread::id m_threadID; // ID
    string m_name;

    mutex m_mutex;  // 互斥锁
    condition_variable m_cond;    // 条件变量
};

