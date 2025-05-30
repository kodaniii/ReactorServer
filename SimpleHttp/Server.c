#include "Server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>

struct FdInfo
{
    int fd;
    int epfd;
    pthread_t tid;
};

int initListenFd(unsigned short port)
{
    // 1. 创建监听的fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return -1;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1)
    {
        perror("setsockopt");
        return -1;
    }
    // 3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr*)&addr, sizeof addr);
    if (ret == -1)
    {
        perror("bind");
        return -1;
    }
    // 4. 设置监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return -1;
    }
    // 返回fd
    return lfd;
}

int epollRun(int lfd)
{
    // 1. 创建epoll实例
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    // 2. lfd添加到epoll中
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
    // 3. 检测
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);
    while (1)
    {
        int num = epoll_wait(epfd, evs, size, -1);
        for (int i = 0; i < num; ++i)
        {
            struct FdInfo* info = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            int fd = evs[i].data.fd;
            info->epfd = epfd;
            info->fd = fd;
            if (fd == lfd)
            {
                // 建立新连接 accept
                // acceptClient(lfd, epfd);
                pthread_create(&info->tid, NULL, acceptClient, info);
            }
            else
            {
                // 主要是接收对端的数据
                // recvHttpRequest(fd, epfd);
                pthread_create(&info->tid, NULL, recvHttpRequest, info);
            }
        }
    }
    return 0;
}

//int acceptClient(int lfd, int epfd)
void* acceptClient(void* arg)
{
    struct FdInfo* info = (struct FdInfo*)arg;
    // 1. 建立连接
    int cfd = accept(info->fd, NULL, NULL);
    if (cfd == -1)
    {
        perror("accept");
        return NULL;
    }
    // 2. 设置非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;     //epoll必须设置为非阻塞模式
    fcntl(cfd, F_SETFL, flag);

    // 3. cfd添加到epoll中
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    //printf("acceptClient.........\n");
    //if (!(ev.events & EPOLLET)) {
    //    printf("events is not EPOLLET!!!!\n");
    //}
    //else {
    //    printf("events is EPOLLET.\n");
    //}
    int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return NULL;
    }
    printf("acceptclient() threadId: %ld\n", info->tid);
    free(info);
    return NULL;
}

//int recvHttpRequest(int cfd, int epfd)
void* recvHttpRequest(void* arg)
{
    struct FdInfo* info = (struct FdInfo*)arg;  //events is not EPOLLET(from Epoll_wait)
    //printf("recvHttpRequest......... \n");
    //if (!(info->fd & EPOLLET)) {
    //    printf("events is not EPOLLET!!!!\n");
    //}
    //else {
    //    printf("events is EPOLLET.\n");
    //}
    int len = 0, totle = 0;
    char tmp[1024] = { 0 };     //recv接收到的数据
    char buf[4096] = { 0 };     //来自客户端HTTP请求的数据
    while ((len = recv(info->fd, tmp, sizeof tmp, 0)) > 0)  // > 0成功接收到数据
    {
        if (totle + len < sizeof buf)
        {
            //memcpy(dst, src, len);
            memcpy(buf + totle, tmp, len);
        }
        totle += len;
    }

    // 判断数据是否被接收完毕
    if (len == -1 && errno == EAGAIN)   //非阻塞I/O下没有更多数据可读
    {
        // 解析请求行，行用\r\n隔开
        char* pt = strstr(buf, "\r\n"); //strstr返回字符串首次出现子串的地址
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf, info->fd);
    }
    else if (len == 0)  //客户端正常关闭连接
    {
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        close(info->fd);
    }
    else
    {
        perror("recv");
    }
    printf("recvHttpRequest() threadId: %ld\n", info->tid);
    free(info);
    return NULL;
}


int parseRequestLine(const char* line, int cfd)
{
    // 解析请求行 get /xxx/1.jpg http/1.1
    char method[12];    //请求方法
    char path[1024];
    //int sscanf(const char *str, const char *format, ...);
    //[^ ]表示除空格以外的任意字符，将前两个由空格分隔开的连续非空格字符序列保存到method和path
    sscanf(line, "%[^ ] %[^ ]", method, path);  //get保存到method /xxx/1.jpg保存到path
    printf("parseRequestLine() method: %s, path: %s\n", method, path);
    //只处理get
    if (strcasecmp(method, "get") != 0) //不区分大小写strcmp
    {
        return -1;
    }
    //解码经过URL编码的请求路径
    decodeMsg(path, path);
    // 处理客户端请求的静态资源(目录或者文件)
    char* file = NULL;
    if (strcmp(path, "/") == 0) //如果是根目录
    {
        file = "./";
    }
    else
    {
        file = path + 1;    //会自动解析成相对路径，自动加上./
    }
    // 获取文件的属性信息
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)  //文件不存在，返回404
    {
        sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        sendFile("404.html", cfd);
        return 0;
    }
    // 判断是目录还是文件
    if (S_ISDIR(st.st_mode))
    {
        sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        sendDir(file, cfd);
    }
    else
    {
        sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        sendFile(file, cfd);
    }

    return 0;
}

/*根据读取的文件后缀，返回HTTP MIME类型，默认为utf-8纯文本*/
const char* getFileType(const char* name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找'.'字符, 如不存在返回NULL
    const char* dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";	// 纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

/*
<html>
    <head>
        <title>title_name</title>
    </head>
    <body>
        <table>
            <tr>
                <td></td>
                <td></td>
            </tr>
            <tr>
                <td></td>
                <td></td>
            </tr>
        </table>
    </body>
</html>
*/
int sendDir(const char* dirName, int cfd)
{
    char buf[4096] = { 0 };
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
    //namelist指针数组，数组保存的是struct dirent*类型，即数组元素类型是目录项的指针
    struct dirent** namelist;
    int num = scandir(dirName, &namelist, NULL, alphasort);
    //处理每个目录项
    for (int i = 0; i < num; ++i)
    {
        char* name = namelist[i]->d_name;   //目录项名称
        struct stat st;
        char subPath[1024] = { 0 };
        sprintf(subPath, "%s/%s", dirName, name);   //完整路径名
        stat(subPath, &st);
        //目录在地址最后面多加了一个"/"
        if (S_ISDIR(st.st_mode))
        {
            //添加行<tr></tr>，列用<td></td>隔开
            //超链接<a href="">name</a>，""为跳转地址，name为可点击的外显文本
            sprintf(buf + strlen(buf),
                "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                name, name, st.st_size);
        }
        else
        {
            sprintf(buf + strlen(buf),
                "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                name, name, st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    free(namelist);
    return 0;
}


int sendFile(const char* fileName, int cfd)
{
    // 1. 打开文件
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
#if 0 
    while (1)
    {
        char buf[1024];
        int len = read(fd, buf, sizeof buf);    //fd->buf
        if (len > 0)
        {
            send(cfd, buf, len, 0);     //将buf发给客户端通信的文件描述符cfd
            usleep(10); // 短暂休息，让接收端能接收过来，否则容易出问题
        }
        else if (len == 0)  //读取完毕
        {
            break;
        }
        else
        {
            perror("read");
        }
    }
#else   //sendfile减少内核态向用户态的切换次数
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    while (offset < size)
    {
        int ret = sendfile(cfd, fd, &offset, size - offset);
        if (ret > 0) {
            printf("sendFile() ret value: %d\n", ret);
        }
        if (ret == -1 && errno == EAGAIN)
        {
            //printf("sendFile Fin.\n");
        }
    }
#endif
    close(fd);
    return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length)
{
    // 状态行
    char buf[4096] = { 0 };
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
    // 响应头
    sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);

    send(cfd, buf, strlen(buf), 0);
    return 0;
}

// 将字符转换为整形数
int hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

// 解码URL编码
// to 存储解码之后的数据, 传出参数, from被解码的数据, 传入参数
void decodeMsg(char* to, char* from)
{
    for (; *from != '\0'; ++to, ++from)
    {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
            // B2 == 178
            // 将3个字符, 变成了一个字符, 这个字符就是原始数据
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

            // 跳过 from[1] 和 from[2] 因此在当前循环中已经处理过了
            from += 2;
        }
        else
        {
            // 字符拷贝, 赋值
            *to = *from;
        }

    }
    *to = '\0';
}
