#ifndef MIN_HEAP
#define MIN_HEAP

#include <iostream>
#include <time.h>
#include <netinet/in.h>
using std::exception;
#define BUFF_SIZE 1024
class heap_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFF_SIZE];
    heap_timer *timer;
};

class heap_timer
{
public:
    heap_timer(int delay) { expire = time(NULL) + delay; }

public:
    time_t expire;
    void (*cb_func)(client_data *);
    client_data *user_data;
};

class time_heap
{
public:
    time_heap(int cap) noexcept(false) : capacity(cap), cur_size(0)
    {
        array = new heap_timer *[capacity];
        if (!array)
        {
            throw std::exception();
        }
        for (int i = 0; i < capacity; i++)
            array[i] = NULL;
    }
    time_heap(heap_timer **init_array, int size, int capacity) noexcept(false) : cur_size(size), capacity(capacity)
    {
        if (capacity < size)
        {
            throw std::exception();
        }
        array = new heap_timer *[capacity];
        if (!array)
        {
            throw std::exception();
        }
        for (int i = 0; i < capacity; i++)
        {
            if (i < size)
                array[i] = init_array[i];
            else
                array[i] = NULL;
        }
        for (int i = (cur_size - 1) / 2; i >= 0; i--)
        {
            // 对数组中第i个元素进行下滤操作
            percolate_down(i);
        }
    }
    ~time_heap()
    {
        for (int i = 0; i < cur_size; i++)
        {
            delete array[i];
        }
        delete[] array;
    }

public:
    void add_timer(heap_timer *timer) noexcept(false)
    {
        if (!timer)
        {
            return;
        }
        if (cur_size >= capacity)
        {
            resize();
        }
        int hole = cur_size++;
        int parent = 0;
        for (; hole > 0; hole = parent)
        {
            parent = (hole - 1) / 2;
            if (array[parent]->expire <= timer->expire)
            {
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }
    void del_timer(heap_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        // 仅仅将回调函数设置为NULL，并不删除定时器，这样将节省真正删除定时器带来的开销，但是容易导致堆数组膨胀
        timer->cb_func = NULL;
    }
    // 获取堆顶元素
    heap_timer *top() const
    {
        if (empty())
        {
            return NULL;
        }
        return array[0];
    }
    // 删除堆顶元素
    void pop_timer()
    {
        if (empty())
        {
            return;
        }
        if (array[0])
        {
            delete array[0];
            array[0] = array[--cur_size];
            percolate_down(0);
        }
    }
    // 心搏函数
    void tick()
    {
        heap_timer *tmp = array[0];
        time_t cur = time(NULL);
        while (!empty())
        {
            if (!tmp)
            {
                break;
            }
            if (tmp->expire > cur)
                break;
            if (array[0]->cb_func)
            {
                array[0]->cb_func(array[0]->user_data);
            }
            pop_timer();
            tmp = array[0];
        }
    }
    bool empty() const { return cur_size == 0; }
    // 调整堆
    void percolate_down(int hole)
    {
        heap_timer *temp = array[hole];
        int child = 0;
        for (; (hole * 2 + 1) < cur_size; hole = child)
        {
            child = hole * 2 + 1;
            if (child < cur_size - 1 && array[child + 1]->expire < array[child]->expire)
            {
                child++;
            }
            if (array[child]->expire < temp->expire)
            {
                array[hole] = array[child];
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }
    void resize() noexcept(false)
    {
        heap_timer **temp = new heap_timer *[2 * capacity];
        capacity = 2 * capacity;
        for (int i = 0; i < capacity; i++)
        {
            if (i < cur_size)
                temp[i] = array[i];
            else
                temp[i] = NULL;
        }
        delete[] array;
        array = temp;
    }

private:
    heap_timer **array;
    int capacity;
    int cur_size;
};

#endif