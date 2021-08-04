//对比索引poll和索引epoll返回的就绪文件描述符

/*
int poll(struct pollfd*fds, nfds_t nfds, int timeout)
struct pollfd
{
    int fd; //文件描述符
    short events; //注册的事件
}   short revents;  //实际发生的事件，由内核填充并返回
*/
int ret = poll(fds, MAX_EVENT_NUMBER, -1); //-1表示poll将阻塞直到事件发生
//必须遍历所有已注册文件描述符并找到其中就绪者
for (int i = 0; i < MAX_EVENT_NUMBER; i++){
    if (fds[i].revents & POLLIN) //POLLIN 表示事件数据可读
    {
        int socketfd = fds[i].fd;
        /*处理socketfd，这里不一定每个socketfd都是就绪的*/
    }
}

//epoll
/*
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) //同select，poll一样，成功时返回就绪文件描述符的数量，失败时返回-1并设置errno
其中epoll_events
struct epoll_event
{
    __uint32_t events;  //epoll事件
    epoll_data_t data;  //用户数据
};
其中
typedef union epoll_data
{
    void* ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
*/
int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
//仅需要遍历ret个文件描述符即可
for (int i = 0; i < ret; i++)
{
    int sockfd = events[i].data.fd;
    /*sockfd肯定已经就绪，直接处理*/
}