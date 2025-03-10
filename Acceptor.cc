#include "Acceptor.h"
#include "Logger.h"
#include "inetAddress.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
}

Acceptor::Acceptor(EventLoop* loop,const InetAddress &listenAddr,bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , AcceptChannel_(loop,acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start() Acceptor.listen 有新用户的连接，要执行一个回调 (confd=>channel=>subloop)
    // baseLoop => acceptChannel_(listenfd) =>
    AcceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this)); 
}

Acceptor::~Acceptor()
{
    AcceptChannel_.disableAll();
    AcceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    AcceptChannel_.enableReading(); // acceptChannel_ => Poller
}

// listenfd有时间发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);    // 轮询找到subLoop，唤醒，纷纷当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d accept reached limit err! \n",__FILE__,__FUNCTION__,__LINE__);
        }

    }
}