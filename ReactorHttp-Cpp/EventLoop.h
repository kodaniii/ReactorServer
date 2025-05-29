#pragma once
#include "Dispatcher.h"
#include "Channel.h"
#include <thread>
#include <queue>
#include <map>
#include <mutex>
using namespace std;

// Channel处理方式
enum class ElemType: char {
    ADD, 
    DELETE, 
    MODIFY
};
// 任务队列元素
struct ChannelElement
{
    ElemType type;   //channel处理方式，ElemType类型
    Channel* channel;
};
class Dispatcher;

class EventLoop
{
public:
    EventLoop();
    EventLoop(const string threadName);
    ~EventLoop();

    // 启动反应堆模型
    int run();

    // 对于单一就绪fd, 调用事件的回调函数(通过channelMap)
    int eventActive(int fd, int event);
    // 添加任务到任务队列
    int addChannelToQ(struct Channel* channel, ElemType type);
    // 将任务队列中的任务分发给dispatcher, 并建立channelmap映射
    int processChannelQ();
    // 释放channel
    int freeChannel(Channel* channel);
    
    // 返回线程ID
    inline thread::id getThreadID()
    {
        return m_threadID;
    }

    inline string getThreadName()
    {
        return m_threadName;
    }

private:
    //向socketpair[0]写数据, 目的是触发socketpair[1]读就绪
    void channelWakeup();
    //从socketpair[1]读数据
    int readMessage();
    static int readLocalMessage(void* arg);

    // 处理任务队列中的节点, 分发给dispatcher(select/poll/epoll), 并建立channelmap映射
    int add(Channel* channel);
    int remove(Channel* channel);
    int modify(Channel* channel);

private:
    bool m_isStop;
    // 线程id, name, mutex
    thread::id m_threadID;
    string m_threadName;
    mutex m_mutex;

    // 该指针指向子类的实例select, poll, epoll 
    Dispatcher* m_dispatcher;
    // 任务队列, 暂存fd以及读/写/读写事件监听请求
    queue<ChannelElement*> m_channelQ;
    // map
    map<int, Channel*> m_channelMap;

    int m_socketPair[2];  // 存储本地通信的fd, 通过socketpair初始化
};


