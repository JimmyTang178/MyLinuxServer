#ifndef LIST_TIMER
#define LIST_TIMER

#include <time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define BUFFER_SIZE 64
class util_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

class util_timer
{
    public:
    util_timer() : prev(NULL), next(NULL) { }
    public:
    time_t expire; //任务的超时时间，这里使用绝对时间
    void (*cb_func) (client_data*); //任务回调函数
    client_data* user_data;  //回调函数处理的客户端数据，由定时器的执行者传递给回调函数
    util_timer* prev;
    util_timer* next;
};

class sort_timer_lst
{
    public:
    sort_timer_lst() : head(NULL), tail(NULL) {}

    ~sort_timer_lst()
    {
        util_timer* tmp = head;
        while(tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    //将目标定时器添加到链表中
    void add_timer(util_timer* timer)
    {
        if (!timer)
        {
            return;
        }
        if (!head)
        {
            head =timer;
            tail = timer;
            return;
        }
        //如果目标定时器的超时时间小于当前链表的所有定时器的超时时间，则把该定时器插入链表头部，作为新的头节点，否则就需要调用重载函数add_timer(util_timer* timer, util_timer* lst_head)，插入到合适的位置，保证链表的升序性、
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }
    //当某个定时任务发生变化时，调整对应的定时器在链表中的位置，这个函数只考虑被调整的定时器的超时时间延长的情况，即该定时器需要往链表尾部移动的情况
    void adjust_timer(util_timer* timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer* tmp = timer->next;
        //如果被调整的目标定时器处在链表尾部，后者该定时器的新的超时值仍然小于其下一个定时器的超时值，则不用调整
        if (!tmp || timer->expire < tmp->expire)
        {
            return;
        }
        //如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表
        if (timer = head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        //如果目标定时器不是链表头节点，则将该定时器从链表中取出，然后插入其所在位置之后的部分链表中
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }
    //将目标定时器timer从链表中删除
    void del_timer(util_timer* timer)
    {
        if (!timer)
        {
            return;
        }
        if (timer == head && timer == tail)
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        //如果链表中至少由两个定时器，且目标定时器是头节点，则将链表的头节点重置为原头节点的下一个节点，然后删除定时器
        else if (timer == head)
        {
            head = head->next;
            head->next->prev = NULL;
            delete timer;
            return;
        }
        //目标定时器是尾结点
        else if (timer == tail)
        {
            tail->prev->next = NULL;
            tail = tail->prev;
            delete timer;
            return;
        }
        //常规情况，timer是链表中间节点
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            delete timer;
            return ;
        }
    }

    //SIGALRM信号每次被触发就在其信号处理函数（如果使用同一事件源，则是主函数）中执行一次tick函数，以处理链表上到期的任务
    void tick()
    {
        if (!head)
        {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(NULL);  //获取系统当前时间
        util_timer* tmp = head;
        //从头节点开始一次处理每个定时器，直到遇到一个未到期的定时器，这是定时器的核心逻辑
        while(tmp)
        {
            //因为每个定时器都是使用绝对时间来作为超时值，所以我们可以把定时器的超时值和系统当前时间比较判断定时器是否到时
            if (cur < tmp->expire)
            {
                break;
            }
            //调用定时器的回调函数，执行定时任务
            tmp->cb_func(tmp->user_data);
            //执行完定时器中的定时任务之后，就将它从链表中删除，并重置链表头节点
            head = tmp->next;
            if (head)
            {
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }
    //private:
    util_timer* head;
    util_timer* tail;
    private:
    //一个重载的辅助函数，被共有的add_timer函数和adjust_timer函数调用，该函数表示将目标定时器timer添加到节点lst_head之后的部分链表中
    void add_timer(util_timer* timer, util_timer* lst_head)
    {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        //遍历lst_head之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间的节点，并将目标定时器插入到该节点之前
        while(tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        //如果遍历完lst_head之后的部分链表依然没有找到超时时间大于目标定时器超时时间的节点，那么就将目标定时器放到链表末尾
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

};

#endif