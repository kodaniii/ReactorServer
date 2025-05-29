#pragma once
#include <functional>

// 定义函数指针
// typedef int(*handleFunc)(void* arg);
// using handleFunc = int(*)(void*);

// 定义文件描述符的读写事件
//强类型枚举
enum class FDEvent
{
    ReadEvent = 0x01,
    WriteEvent = 0x02
};

class Channel
{
public:
    // 可调用对象包装器包装函数指针或可调用对象
    using handleFunc = std::function<int(void*)>;
    Channel(int fd, FDEvent events, handleFunc readFunc, 
        handleFunc writeFunc, handleFunc destroyFunc, void* arg);
    // 回调函数
    handleFunc readCallback;
    handleFunc writeCallback;
    handleFunc destroyCallback;
    // fd修改监听写事件
    void writeEventEnable(bool flag);
    // fd是否要求检测写事件
    bool isWriteEventEnable();
    inline int getEvent()
    {
        return m_events;
    }
    inline int getSocket()
    {
        return m_fd;
    }
    inline const void* getArg()
    {
        return m_arg;
    }
private:
    // 文件描述符
    int m_fd;
    // 事件
    int m_events;
    // 回调函数的参数
    void* m_arg;
};

