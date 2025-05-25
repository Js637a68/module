#include "myngx_alloc.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ngx_log_error_core(ngx_uint_t level,
                        const char *fmt, ...)
{
}

typedef struct Data stData;
struct Data
{
    char *ptr;
    FILE *pfile;
};

void func1(void *p1)
{
    char *p = (char *)p1;
    printf("free ptr mem!");
    free(p);
}
void func2(void *pf1)
{
    FILE *pf = (FILE *)pf1;
    printf("close file!");
    fclose(pf);
}
void test()
{
    // 512 - sizeof(ngx_pool_t) - 4095   =>   max
    ngx_mem_pool pool;

    if (!pool.ngx_create_pool(512))
    {
        printf("ngx_create_pool fail...");
        return;
    }

    void *p1 = pool.ngx_palloc(128); // 从小块内存池分配的
    if (p1 == NULL)
    {
        printf("ngx_palloc 128 bytes fail...");
        return;
    }

    stData *p2 = (stData *)pool.ngx_palloc(512); // 从大块内存池分配的
    if (p2 == NULL)
    {
        printf("ngx_palloc 512 bytes fail...");
        return;
    }
    p2->ptr = (char *)malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");

    ngx_pool_cleanup_t *c1 = pool.ngx_pool_cleanup_add(0);
    c1->handler = func1;
    c1->data = p2->ptr;

    ngx_pool_cleanup_t *c2 = pool.ngx_pool_cleanup_add(0);
    c2->handler = func2;
    c2->data = p2->pfile;

    pool.ngx_destroy_pool(); // 1.调用所有的预置的清理函数 2.释放大块内存 3.释放小块内存池所有内存

    return;
}

int main()
{
    test();
    return 0;
}