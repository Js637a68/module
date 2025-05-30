#ifndef MYSGIALLOC_H
#define MYSGIALLOC_H

#include <iostream>
#include <cstring>
#include <mutex>

static void (*__malloc_alloc_oom_handler)() = 0;
void *__malloc_alloc(size_t __n)
{
    void *__result = malloc(__n);
    if (0 == __result)
    {
        void (*__my_malloc_handler)();
        for (;;)
        {
            __my_malloc_handler = __malloc_alloc_oom_handler;
            if (0 == __my_malloc_handler)
                throw std::bad_alloc();
            (*__my_malloc_handler)();
            __result = malloc(__n);
            if (__result)
                return __result;
        }
    }
    return __result;
}

/*
预分配+动态扩容机制
*/
template <typename T>
class mysgialloc
{
public:
    using value_type = T;
    T *allocate(size_t __n)
    {
        //__n = sizeof(T) * __n;
        void *__ret = 0;

        if (__n > (size_t)_MAX_BYTES)
        {
            __ret = malloc(__n);
        }
        else
        {
            /* 这里__my_free_list存放着Obj*的地址，Obj*存放着_Obj的地址，
            并且volatile修饰的是Obj*, 跟const类似
            */
            _Obj *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);

            std::lock_guard<std::mutex> lock(mtx);
            _Obj *__result = *__my_free_list;
            if (__result == 0)
            {
                __ret = _S_refill(_S_round_up(__n));
            }
            else
            {
                *__my_free_list = __result->_M_free_list_link;
                __ret = __result;
            }
        }
        return (T *)__ret;
    }

    void deallocate(void *__p, size_t __n)
    {
        if (__n > (size_t)_MAX_BYTES)
        {
            free(__p);
        }
        else
        {
            _Obj *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);
            _Obj *__q = (_Obj *)__p;

            std::lock_guard<std::mutex> lock(mtx);
            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
        }
    }

    void *reallocate(void *__p, size_t __old_sz, size_t __new_sz)
    {
        void *__result;
        size_t __copy_sz;

        if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES)
        {
            return (realloc(__p, __new_sz));
        }
        if (_S_round_up(__old_sz) == _S_round_up(__new_sz))
            return (__p);
        __result = allocate(__new_sz);
        __copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
        memcpy(__result, __p, __copy_sz);
        deallocate(__p, __old_sz);
        return __result;
    }

    void constract(T *__p, const T &val)
    {
        new (__p) T(val);
    }

    void destroy(T *__p)
    {
        __p->~T();
    }

private:
    enum
    {
        _ALLGN = 8
    };
    enum
    {
        _MAX_BYTES = 128
    };
    enum
    {
        _NFREELISTS = _MAX_BYTES / _ALLGN
    };

    union _Obj
    {
        union _Obj *_M_free_list_link;
        char _M_client_data[1];
    };

    static _Obj *volatile _S_free_list[_NFREELISTS];

    static char *_S_start_free;
    static char *_S_end_free;
    static size_t _S_heap_size;

    static std::mutex mtx;

    static size_t _S_round_up(size_t bytes)
    {
        return ((bytes + _ALLGN - 1) & ~(_ALLGN - 1));
    }
    static size_t _S_freelist_index(size_t _bytes)
    {
        return ((_bytes + _ALLGN - 1) / (size_t)_ALLGN - 1);
    }

    static void *_S_refill(size_t __n)
    {
        int __nobjs = 20;
        char *__chunk = _S_chunk_alloc(__n, __nobjs);
        _Obj *volatile *__my_free_list = _S_free_list + _S_freelist_index(__n);
        _Obj *__result;
        _Obj *__current_obj;
        _Obj *__next_obj;

        if (1 == __nobjs)
            return __chunk;

        __result = (_Obj *)__chunk;
        *__my_free_list = __next_obj = (_Obj *)(__chunk + __n);
        for (int __i = 1;; ++__i)
        {
            __current_obj = __next_obj;
            __next_obj = (_Obj *)((char *)__next_obj + __n);
            if (__nobjs - 1 == __i)
            {
                __current_obj->_M_free_list_link = 0;
                break;
            }
            else
            {
                __current_obj->_M_free_list_link = __next_obj;
            }
        }
        return __result;
    }

    static char *_S_chunk_alloc(size_t __size, int &__nobjs)
    {
        char *__result;
        size_t __total_bytes = __size * __nobjs;
        size_t __bytes_left = _S_end_free - _S_start_free;

        if (__bytes_left >= __total_bytes)
        {
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return __result;
        }
        else if (__bytes_left >= __size)
        {
            __nobjs = (int)(__bytes_left / __size);
            __total_bytes = __size * __nobjs;
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return __result;
        }
        else
        {
            size_t __bytes_to_get =
                2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
            if (__bytes_left > 0)
            {
                _Obj *volatile *__my_free_list =
                    _S_free_list + _S_freelist_index(__bytes_left);

                ((_Obj *)_S_start_free)->_M_free_list_link = *__my_free_list;
                *__my_free_list = (_Obj *)_S_start_free;
            }
            _S_start_free = (char *)malloc(__bytes_to_get);
            if (0 == _S_start_free)
            {
                size_t __i;
                _Obj *volatile *__my_free_list;
                _Obj *__p;
                for (__i = __size; __i <= (size_t)_MAX_BYTES; __i += (size_t)_ALLGN)
                {
                    __my_free_list = _S_free_list + _S_freelist_index(__i);
                    __p = *__my_free_list;
                    if (0 != __p)
                    {
                        *__my_free_list = __p->_M_free_list_link;
                        _S_start_free = (char *)__p;
                        _S_end_free = _S_start_free + __i;
                        return (_S_chunk_alloc(__size, __nobjs));
                    }
                }
                _S_end_free = 0;
                _S_start_free = (char *)malloc(__bytes_to_get);
            }
            _S_heap_size += __bytes_to_get;
            _S_end_free = _S_start_free + __bytes_to_get;
            return (_S_chunk_alloc(__size, __nobjs));
        }
    }
};

template <typename T>
typename mysgialloc<T>::_Obj *volatile mysgialloc<T>::_S_free_list[mysgialloc<T>::_NFREELISTS] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};
template <typename T>
char *mysgialloc<T>::_S_start_free = nullptr;
template <typename T>
char *mysgialloc<T>::_S_end_free = nullptr;
template <typename T>
size_t mysgialloc<T>::_S_heap_size = 0;
template <typename T>
std::mutex mysgialloc<T>::mtx;

#endif
