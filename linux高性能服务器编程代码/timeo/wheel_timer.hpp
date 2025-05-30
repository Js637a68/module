#ifndef TIME_WHEEL_TIMER_H
#define TIME_WHEEL_TIMER_H
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFFER_SIZE 64

class tw_timer;
class client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer *timer;
};

class tw_timer
{
public:
    tw_timer(int rot, int ts) : rotation(rot), time_slot(ts) {}

public:
    int rotation;                   // 记录定时器在时间轮转多少圈后生效
    int time_slot;                  // 记录定时器属于时间轮上的哪个槽（哪个时间区间）
    void (*cb_func)(client_data *); // 定时器回调函数
    client_data *user_data;         // 用户数据
    tw_timer *next;                 // 指向下一个定时器
    tw_timer *prev;                 // 指向上一个定时器
};

class time_wheel
{
public:
    time_wheel() : cur_slot(0)
    {
        for (int i = 0; i < N; ++i)
        {
            slots[i] = NULL;
        }
    }
    ~time_wheel()
    {
        for (int i = 0; i < N; ++i)
        {
            tw_timer *tmp = slots[i];
            while (tmp)
            {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }
    // 根据定时值创建定时器，并插入到时间轮中
    tw_timer *add_timer(int timeout)
    {
        if (timeout < 0)
        {
            return NULL;
        }
        int ticks = 0;
        // 下面根据timeout的值计算出定时器在时间轮转动多少圈后触发，以及定时器应该被插入到时间轮的哪个槽中
        if (timeout < SI)
        {
            ticks = 1;
        }
        else
        {
            ticks = timeout / SI;
        }
        // 计算待插入的定时器在时间轮转动多少圈后触发
        int rotation = ticks / N;
        // 计算待插入的定时器应该被插入到时间轮的哪个槽中
        int ts = (cur_slot + (ticks % N)) % N;
        // 创建新的定时器，它在时间轮转动rotation圈后，且在时间轮的ts槽中
        tw_timer *timer = new tw_timer(rotation, ts);
        // 如果时间轮的ts槽中尚无任何定时器，则把新建的定时器插入其中，并成为头节点
        if (!slots[ts])
        {
            printf("add timer, rotation is %d, ts is %d\n", rotation, ts);
            slots[ts] = timer;
        }
        // 否则把该定时器插入到链表中
        else
        {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }
    // 删除目标定时器
    void del_timer(tw_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        int ts = timer->time_slot;
        // slots[ts]是目标定时器所在槽的头节点，如果目标定时器就是该头节点，则需要重新链接头节点
        if (timer == slots[ts])
        {
            slots[ts] = slots[ts]->next;
            if (slots[ts])
            {
                slots[ts]->prev = NULL;
            }
            delete timer;
        }
        else
        {
            timer->prev->next = timer->next;
            if (timer->next)
            {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }
    // SI时间到了，调用该函数，时间轮向前滚动一格
    void tick()
    {
        tw_timer *tmp = slots[cur_slot]; // 取得时间轮上当前槽的头节点
        printf("current slot is %d\n", cur_slot);
        while (tmp)
        {
            printf("tick the timer once\n");
            // 如果定时器的rotation值大于0，则它在这一轮不起作用
            if (tmp->rotation > 0)
            {
                tmp->rotation--;
                tmp = tmp->next;
            }
            else
            {
                // 否则，说明定时器已经到期，于是执行定时任务，然后删除该定时器
                tmp->cb_func(tmp->user_data);
                if (tmp == slots[cur_slot])
                {
                    printf("delete header in cur_slot\n");
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if (slots[cur_slot])
                    {
                        slots[cur_slot]->prev = NULL;
                    }
                    tmp = slots[cur_slot];
                }
                else
                {
                    tmp->prev->next = tmp->next;
                    if (tmp->next)
                    {
                        tmp->next->prev = tmp->prev;
                    }
                    tw_timer *tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        cur_slot = (cur_slot + 1) % N;
    }

private:
    static const int N = 60; // 时间轮上槽的数目
    static const int SI = 1; // 每1s时间轮转动一格
    int cur_slot;            // 时间轮的当前槽，范围是0-N-1，初始值为0
    tw_timer *slots[N];      // 时间轮的槽，其中每个元素指向一个定时器链表，链表无序
};

#endif