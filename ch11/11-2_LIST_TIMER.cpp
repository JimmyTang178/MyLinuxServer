//测试11-2_LIST_TIMER.h是否正确
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "11-2_LIST_TIMER.h"

int main(int argc, char*argv[])
{
    if (argc < 2)
    {
        printf("usage: %s timers\n", argv[0]);
        return 1;
    }
    int timers = atoi(argv[1]);
    time_t timeouts[timers];
    int i = 0;
    sort_timer_lst stl;
    printf("stl.head:%d\n", stl.head);
    printf("stl.tail:%d\n", stl.tail);
    while(i < timers)
    {
        scanf("%ld", &timeouts[i]);
        util_timer* ut = new util_timer; //如果是util_timer ut这样做的话，会导致反复添加同一个对象ut，使得链表中只有一个节点
        ut->user_data = NULL;
        ut->expire = timeouts[i];
        stl.add_timer(ut);
        printf("adding stl.head:%d\n", stl.head->expire);
        printf("adding stl.tail:%d\n", stl.tail->expire);
        //printf("timer:%d\n", ut->expire);
        i++;
    }
    util_timer* tmp = stl.head;
    printf("stl.head value:%d \n", stl.head->expire);
    printf("stl.tail value:%d \n", stl.tail->expire);
    printf("Traversing timer list...\n");
    while(tmp)
    {
        printf("%ld\n", tmp->expire);
        tmp = tmp->next;
    }
    return 0;
}
/*
stl.head:0
stl.tail:0
5
adding stl.head:5
adding stl.tail:5
63
adding stl.head:5
adding stl.tail:63
1
adding stl.head:1
adding stl.tail:63
2
adding stl.head:1
adding stl.tail:63
2
adding stl.head:1
adding stl.tail:63
stl.head value:1 
stl.tail value:63 
Traversing timer list...
1
2
2
5
63
*/