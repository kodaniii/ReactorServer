#pragma once
#include <string>
#include <mutex>

using namespace std;

class Buffer
{
public:
    Buffer(int size);
    ~Buffer();

    // 缓冲区扩容
    void extendRoom(int size);

    // 写内存
    int writeToBuffer(const char* data, int size);
    int writeToBuffer(const char* data);
    int writeToBuffer(const string data);
    int socketRead(int fd);

    // 根据\r\n取出一行, 找到其在数据块中的位置, 返回该位置
    char* findCRLF();
    // 发送数据
    int sendData(int socket);    // 指向内存的指针
    // 得到读数据的起始位置
    inline char* data()
    {
        //printf("data { %s }", m_data);
        return m_data + m_readPos;
    }
    inline int readPosIncrease(int count)
    {
        m_readPos += count;
        return m_readPos;
    }

    
    // 已读的内存容量(可释放)
    inline int usedSize()
    {
        return m_readPos;
    }
    // 剩余可写的内存容量
    inline int writeableSize()
    {
        return m_capacity - m_writePos;
    }
    // 剩余的可读的内存容量
    inline int readableSize()
    {
        return m_writePos - m_readPos;
    }

private:
    char* m_data;       //缓冲区始址
    int m_capacity;     //总容量
    int m_readPos = 0;  //读指针相对始址偏移量
    int m_writePos = 0; //写指针相对始址偏移量
};

