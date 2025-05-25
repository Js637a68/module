#include "mysgi_alloc.h"
#include <vector>
int main()
{
    std::vector<int, mysgialloc<int>> v;
    for (int i = 0; i < 100; i++)
    {
        v.push_back(i);
    }
    for (int i = 0; i < 100; i++)
    {
        printf("%d ", v[i]);
    }
}