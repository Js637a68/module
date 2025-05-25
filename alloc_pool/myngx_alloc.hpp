#ifndef MYNGX_ALLOC_H
#define MYNGX_ALLOC_H
#include <cstdlib>
#include <cstring>
using u_char = unsigned char;
using ngx_uint_t = unsigned int;
using size_t = unsigned long;
using uintptr_t = unsigned long;

#define ngx_align(d, a) (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a) (u_char *)(((uintptr_t)(p) + ((uintptr_t)a - 1)) & ~((uintptr_t)a - 1))
#define NGX_ALLIGNMENT sizeof(unsigned long)
#define ngx_memzero(p, n) memset(p, 0, n)

/* 清理非POD类型的回调函数 */
typedef void (*ngx_pool_cleanup_pt)(void *data);
struct ngx_pool_cleanup_t
{
    ngx_pool_cleanup_pt handler;
    void *data;
    ngx_pool_cleanup_t *next;
};

struct ngx_pool_t;
/* 大块内存的头部信息*/
struct ngx_pool_large_t
{
    ngx_pool_large_t *next;
    void *alloc;
};
/* 内存池的头部信息 */
struct ngx_pool_data_t
{
    u_char *last;
    u_char *end;
    ngx_pool_t *next;
    ngx_uint_t failed;
};

struct ngx_pool_t
{
    ngx_pool_data_t d;
    size_t max;
    ngx_pool_t *current;
    ngx_pool_large_t *large;
    ngx_pool_cleanup_t *cleanup;
};

const int ngx_pagesize = 4096;
const int NGX_MAX_ALLOC_FROM_POOL = (ngx_pagesize - 1);
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
const int NGX_POOL_ALIGNMENT = 16;
const int NGX_MIN_POOL_SIZE = ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)), NGX_POOL_ALIGNMENT);

class ngx_mem_pool
{
public:
    bool ngx_create_pool(size_t size);
    void *ngx_palloc(size_t size);  // 考虑到对齐，从内存池分配size大小的内存
    void *ngx_pnalloc(size_t size); // 不考虑对齐，从内存池分配size大小的内存
    void *ngx_pcalloc(size_t size); // 考虑到对齐，从内存池分配size大小的内存，并将内存清零
    void ngx_free(void *p);         // 释放大块内存
    void ngx_reset_pool();
    void ngx_destroy_pool();
    ngx_pool_cleanup_t *ngx_pool_cleanup_add(size_t size);

private:
    ngx_pool_t *m_pool;

    void *ngx_palloc_small(size_t size, ngx_uint_t align); // 小块内存分配
    void *ngx_palloc_large(size_t size);                   // 大块内存分配
    void *ngx_palloc_block(size_t size);                   // 分配新的小块内存池
};

bool ngx_mem_pool::ngx_create_pool(size_t size)
{
    ngx_pool_t *p;

    p = (ngx_pool_t *)malloc(size);
    if (p == nullptr)
    {
        return false;
    }

    p->d.last = (u_char *)p + sizeof(ngx_pool_t);
    p->d.end = (u_char *)p + size;
    p->d.next = nullptr;
    p->d.failed = 0;
    size = size - sizeof(ngx_pool_t);
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->large = nullptr;
    p->cleanup = nullptr;
    m_pool = p;

    return true;
}

void *ngx_mem_pool::ngx_palloc(size_t size)
{
    if (size <= m_pool->max)
    {
        return ngx_palloc_small(size, 1);
    }
    else
    {
        return ngx_palloc_large(size);
    }
}
void *ngx_mem_pool::ngx_pnalloc(size_t size)
{
    if (size <= m_pool->max)
    {
        return ngx_palloc_small(size, 0);
    }
    else
    {
        return ngx_palloc_large(size);
    }
}

void *ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
    u_char *m;
    ngx_pool_t *p;
    p = m_pool->current;
    do
    {
        m = p->d.last;
        if (align)
        {
            m = (u_char *)ngx_align_ptr(m, NGX_ALLIGNMENT);
        }

        if ((size_t)(p->d.end - m) >= size)
        {
            p->d.last = m + size;
            return m;
        }
        p = p->d.next;
    } while (p);
    return ngx_palloc_block(size);
}

void *ngx_mem_pool::ngx_palloc_block(size_t size)
{
    u_char *m;
    size_t psize;
    ngx_pool_t *p, *new_pool;

    psize = (size_t)(m_pool->d.end - (u_char *)m_pool);
    m = (u_char *)malloc(psize);
    if (m == nullptr)
        return nullptr;

    new_pool = (ngx_pool_t *)m;
    new_pool->d.end = m + psize;
    new_pool->d.next = nullptr;
    new_pool->d.failed = 0;
    m += sizeof(ngx_pool_t);
    m = ngx_align_ptr(m, NGX_ALLIGNMENT);
    new_pool->d.last = m + size;
    for (p = m_pool->current; p->d.next; p = p->d.next)
    {
        if (p->d.failed++ > 4)
        {
            m_pool->current = p->d.next;
        }
    }
    p->d.next = new_pool;
    return m;
}

void *ngx_mem_pool::ngx_palloc_large(size_t size)
{
    void *p;
    ngx_uint_t n;
    ngx_pool_large_t *large;
    p = malloc(size);
    if (p == nullptr)
        return nullptr;

    n = 0;
    for (large = m_pool->large; large; large = large->next)
    {
        if (large->alloc == nullptr)
        {
            large->alloc = p;
            return p;
        }

        if (n++ > 3)
        {
            break;
        }
    }
    large = (ngx_pool_large_t *)ngx_palloc_small(sizeof(ngx_pool_large_t), 1);
    if (large == nullptr)
    {
        free(p);
        return nullptr;
    }
    large->alloc = p;
    large->next = m_pool->large;
    m_pool->large = large;
    return p;
}
void ngx_mem_pool::ngx_free(void *p)
{
    ngx_pool_large_t *l;
    for (l = m_pool->large; l; l = l->next)
    {
        if (l->alloc == p)
        {
            free(l->alloc);
            l->alloc = nullptr;
            return;
        }
    }
}

void *ngx_mem_pool::ngx_pcalloc(size_t size)
{
    void *p;
    p = ngx_palloc(size);
    if (p)
    {
        ngx_memzero(p, size);
    }
    return p;
}

void ngx_mem_pool::ngx_reset_pool()
{
    ngx_pool_t *p;
    ngx_pool_large_t *l;
    for (l = m_pool->large; l; l = l->next)
    {
        if (l->alloc)
        {
            free(l->alloc);
            l->alloc = nullptr;
        }
    }

    p = m_pool;
    p->d.last = (u_char *)p + sizeof(ngx_pool_t);
    p->d.failed = 0;

    for (p = p->d.next; p; p = p->d.next)
    {
        p->d.last = (u_char *)p + sizeof(ngx_pool_t);
        p->d.failed = 0;
    }

    m_pool->current = m_pool;
    m_pool->large = nullptr;
}

void ngx_mem_pool::ngx_destroy_pool()
{
    ngx_pool_t *p, *n;
    ngx_pool_large_t *l;
    ngx_pool_cleanup_t *c;

    for (c = m_pool->cleanup; c; c = c->next)
    {
        if (c->handler)
        {
            c->handler(c->data);
        }
    }

    for (l = m_pool->large; l; l = l->next)
    {
        if (l->alloc)
        {
            free(l->alloc);
        }
    }

    for (p = m_pool, n = p->d.next; /* void */; p = n, n = n->d.next)
    {
        free(p);
        if (n == nullptr)
        {
            break;
        }
    }
}

ngx_pool_cleanup_t *ngx_mem_pool::ngx_pool_cleanup_add(size_t size)
{
    ngx_pool_cleanup_t *c;

    c = (ngx_pool_cleanup_t *)ngx_palloc(sizeof(ngx_pool_cleanup_t));
    if (c == nullptr)
    {
        return nullptr;
    }
    if (size)
    {
        c->data = ngx_palloc(size);
        if (c->data == nullptr)
        {
            return nullptr;
        }
    }
    else
    {
        c->data = nullptr;
    }

    c->handler = nullptr;
    c->next = m_pool->cleanup;
    m_pool->cleanup = c;
    return c;
}

#endif