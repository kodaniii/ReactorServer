#include "TcpConnection.h"
#include "HttpRequest.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"

TcpConnection::TcpConnection(int fd, EventLoop* evloop)
{
    m_evLoop = evloop;
    m_readBuf = new Buffer(10240);
    m_writeBuf = new Buffer(10240);
    // http
    m_request = new HttpRequest;
    m_response = new HttpResponse;
    m_name = string("Connection_") + to_string(fd);
    m_channel = new Channel(fd, FDEvent::ReadEvent, processRead, processWrite, destroy, this);
    evloop->addChannelToQ(m_channel, ElemType::ADD);
    Debug("TcpConnection() connName: %s", m_name.c_str());
}

TcpConnection::~TcpConnection()
{
    Debug("~TcpConnection() connName: %s", m_name.c_str());
    if (m_readBuf && m_readBuf->readableSize() == 0 &&
        m_writeBuf && m_writeBuf->readableSize() == 0)
    {
        delete m_readBuf;
        delete m_writeBuf;
        delete m_request;
        delete m_response;
        m_evLoop->freeChannel(m_channel);
    }
}

int TcpConnection::processRead(void* arg)
{
    TcpConnection* conn = static_cast<TcpConnection*>(arg);
    // 接收数据
    int socket = conn->m_channel->getSocket();
    int count = conn->m_readBuf->socketRead(socket);

    Debug("TcpConnection::processRead() cnt %d\n%s", count, conn->m_readBuf->data());
    if (count > 0)
    {
        // 接收到了http请求, 解析http请求
#ifdef MSG_SEND_AUTO
        conn->m_channel->writeEventEnable(true);
        conn->m_evLoop->addChannelToQ(conn->m_channel, ElemType::MODIFY);
#endif
        bool flag = conn->m_request->parseHttpRequest(
            conn->m_readBuf, conn->m_response,
            conn->m_writeBuf, socket);
        if (!flag)
        {
            // 解析失败, 回复一个简单的html
            string errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            conn->m_writeBuf->writeToBuffer(errMsg);
        }
    }
    else
    {
#ifdef MSG_SEND_AUTO
        // 断开连接
        conn->m_evLoop->addChannelToQ(conn->m_channel, ElemType::DELETE);
#endif

    }
#ifndef MSG_SEND_AUTO
    // 断开连接
    conn->m_evLoop->addChannelToQ(conn->m_channel, ElemType::DELETE);
#endif
    return 0;
}

int TcpConnection::processWrite(void* arg)
{
    TcpConnection* conn = static_cast<TcpConnection*>(arg);
    // 发送数据
    int count = conn->m_writeBuf->sendData(conn->m_channel->getSocket());
    if (count > 0)
    {
        // 判断数据是否被全部发送出去了
        if (conn->m_writeBuf->readableSize() == 0)
        {
            // 1. 不再检测写事件 -- 修改channel中保存的事件
            conn->m_channel->writeEventEnable(false);
            // 2. 修改dispatcher检测的集合 -- 添加任务节点
            conn->m_evLoop->addChannelToQ(conn->m_channel, ElemType::MODIFY);
            // 3. 删除这个节点
            conn->m_evLoop->addChannelToQ(conn->m_channel, ElemType::DELETE);
        }
    }
    Debug("TcpConnection::processWrite() cnt %d", count);
    return 0;
}

int TcpConnection::destroy(void* arg)
{
    TcpConnection* conn = static_cast<TcpConnection*>(arg);
    if (conn != nullptr)
    {
        delete conn;
    }
    return 0;
}

