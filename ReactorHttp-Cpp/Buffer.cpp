//#define _GNU_SOURCE
#include "Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>
#include "Log.h"

Buffer::Buffer(int size): m_capacity(size)
{
    m_data = new char[size]();
    //m_data = (char*)malloc(size);
    //bzero(m_data, size);
}

Buffer::~Buffer()
{
    // if (m_data != nullptr)
    // {
    //     free(m_data);
    // }
    delete[] m_data;
}

//buffer = 写入已被读(相当于已使用过, 可释放)+写入但未读(等待被读)+未读写(可写)
void Buffer::extendRoom(int size)
{
    // 1. 内存够用 - 不需要扩容
    if (writeableSize() >= size)
    {
        return;
    }
    // 2. 内存需要合并才够用 - 不需要扩容
    // 未读写(可写)的内存 + 已读的内存(已使用过) > size
    else if (usedSize() + writeableSize() >= size)
    {
        // 得到未读(可读)的内存大小
        int readable = readableSize();
        // 移动内存
        memcpy(m_data, m_data + m_readPos, readable);
        // 更新位置
        m_readPos = 0;
        m_writePos = readable;
    }
    // 3. 内存不够用 - 扩容
    else
    {
        // 增量扩容: 每次增加size大小的容量, 以减少扩容次数
        void* temp = realloc(m_data, m_capacity + size);
        if (temp == NULL)
        {
            return;
        }
        memset((char*)temp + m_capacity, 0, size);
        // 更新数据
        m_data = static_cast<char*>(temp);
        m_capacity += size;
    }
}

int Buffer::writeToBuffer(const char* data, int size)
{
    if (data == nullptr || size <= 0)
    {
        return -1;
    }
    // 扩容
    extendRoom(size);
    // 数据拷贝
    memcpy(m_data + m_writePos, data, size);
    m_writePos += size;
    return 0;
}

int Buffer::writeToBuffer(const char* data)
{
    int size = strlen(data);
    int ret = writeToBuffer(data, size);
    return ret;
}

int Buffer::writeToBuffer(const string data)
{
    int ret = writeToBuffer(data.data());
    return ret;
}

/*
    通过iovec+readv/writev实现分散读+聚集写, 减少内存拷贝,
    通过一次系统调用, 将数据保存到buffer和临时buffer,
    如果用read, 当buffer空间不足时, 还需要二次调用read读到临时buffer
*/
int Buffer::socketRead(int fd)
{
    // 分散读+聚集写
    struct iovec vec[2];
    // 初始化数组元素
    int writeable = writeableSize();
    vec[0].iov_base = m_data + m_writePos;
    vec[0].iov_len = writeable;
    //TODO: 目前存在的问题是频繁的malloc, 降低性能、产生内存碎片, 可考虑搞一个内存池
    char* tmpbuf = (char*)malloc(40960);    //40KB, 承接大文件的溢出数据
    vec[1].iov_base = tmpbuf;
    vec[1].iov_len = 40960;

    int res = readv(fd, vec, 2);
    printf("socketRead() errno EAGAIN %d res %d\n", errno == EAGAIN, res);
    if (res == -1)
    {
        free(tmpbuf);
        perror("readv");
        return -1;
    }
    if (res <= writeable)
    {
        m_writePos += res;
    }
    else
    {
        m_writePos = m_capacity;
        writeToBuffer(tmpbuf, res - writeable); //扩容buffer并将tmpbuf写入
    }
    //printf("Read fd %d %d bytes\n", fd, res);  // 打印读取的字节数
    free(tmpbuf);
    return res;
}

char* Buffer::findCRLF()
{
    //ptr指向'\r'
    char* ptr = (char*)memmem(m_data + m_readPos, readableSize(), "\r\n", 2);
    return ptr;
}

int Buffer::sendData(int socket)
{
    //当前可发送大小
    int readable = readableSize();
    if (readable <= 0) {
        return 0;
    }
    int totalSent = 0;
    const char* dataPtr = m_data + m_readPos;   //读取起始位置

    while (readable > totalSent)
    {
        int count = send(socket, dataPtr + totalSent, readable - totalSent, MSG_NOSIGNAL);
        totalSent += count;
        Debug("Buffer::sendData send value %d bytes\n", m_readPos);
    }
    m_readPos += totalSent;
    return 0;


    // //old version
    // // 判断有无数据
    // int readable = readableSize();
    // if (readable > 0)
    // {
    //     int count = send(socket, m_data + m_readPos, readable, MSG_NOSIGNAL);
    //     if (count > 0)
    //     {
    //         m_readPos += count;
    //         usleep(1);
    //     }
    //     Debug("Buffer::sendData send value %d bytes\n", m_readPos);
    //     return count;
    // }
    // return 0;
}