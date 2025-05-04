#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <time.h>
#include <arpa/inet.h>
#include <stdio.h>
#define BUFFER_SIZE 64

class util_timer;

// 结构为地址，套接字，缓冲区，定时器
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

// 定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;                  // 任务的超时时间，这里使用绝对时间
    void (*cb_func)(client_data *); // 任务回调函数
    client_data *user_data;         // 用户数据
    util_timer *prev;               // 指向上一个定时器
    util_timer *next;               // 指向下一个定时器
};

class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst()
    {
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    // 将目标定时器timer添加到链表中
    void add_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if (head == NULL)
        {
            head = tail = timer;
            return;
        }
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }
    // 调整定时器，任务发生变化时，调整定时器在链表中的位置
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer *tmp = timer->next;
        // 如果被调整的目标定时器处在链表尾部，或者该定时器新的超时值仍然小于下一个定时器的超时值，则不调整
        if (!tmp || (timer->expire < tmp->expire))
        {
            return;
        }
        // 如果目标定时器是链表头节点，则将该定时器从链表中取出并重新插入链表
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else
        {
            // 如果目标定时器不是链表头节点，则将该定时器从链表中取出，并重新插入链表
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }
    // 删除目标定时器
    void del_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        // 如果链表中只有一个定时器，则将head和tail都置为NULL
        if (timer == head && timer == tail)
        {
            delete timer;
            head = tail = NULL;
            return;
        }
        // 如果链表中至少有两个定时器，且目标定时器是链表头节点，则将head指向第二个定时器
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        // 如果链表中至少有两个定时器，且目标定时器是链表尾节点，则将tail指向倒数第二个定时器，并将其next设置为NULL
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        // 如果链表中至少有两个定时器，且目标定时器既不是链表头节点也不是链表尾节点，则将目标定时器的前后定时器串联起来，然后删除目标定时器
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
    // SIGALARM信号每次被触发就在其信号处理函数中执行一次tick函数，以处理链表上到期任务
    void tick()
    {
        if (!head)
        {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(NULL); // 获取当前时间
        util_timer *tmp = head;
        // 从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
        while (tmp)
        {
            // 因为每个定时器都使用绝对时间作为超时值，所以我们可以把定时器的超时值和当前时间作比较，以判断定时器是否到期
            if (cur < tmp->expire)
            {
                break;
            }
            // 调用定时器的回调函数，以执行定时任务
            tmp->cb_func(tmp->user_data);
            // 执行完定时任务后，将该定时器从链表中删除，并重置链表头节点
            head = tmp->next;
            if (head)
            {
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    // 一个重载的辅助函数，它被公有的add_timer函数和adjust_timer函数调用
    // 该函数表示将目标定时器timer添加到节点lst_head之后的部分链表中
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        // 遍历lst_head节点之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间节点
        // 并将目标定时器插入该节点之前
        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                timer->prev = prev;
                tmp->prev = timer;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        // 如果遍历完lst_head节点之后的部分链表，仍未找到超时时间大于目标定时器的超时时间的节点，则将目标定时器插入链表尾部，并把它设置为链表新的尾节点
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};
#endif