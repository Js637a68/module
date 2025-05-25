#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int a = 0;
int b = 0;
pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void *another(void *arg)
{
    pthread_mutex_lock(&mutex_b);
    printf("in thread b, got mutex b, waiting for mutex a\n");
    sleep(1);
    ++b;
    pthread_mutex_lock(&mutex_a);
    b += a++;
    pthread_mutex_unlock(&mutex_a);
    pthread_mutex_unlock(&mutex_b);
    pthread_exit(NULL);
}

int main()
{
    pthread_t tid;
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);
    pthread_create(&tid, NULL, another, NULL);
    pthread_mutex_lock(&mutex_a);
    printf("in main thread, get mutex a, waiting for mutex b\n");
    sleep(1);
    ++a;
    pthread_mutex_lock(&mutex_b);
    a += b++;
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&mutex_a);
    pthread_mutex_destroy(&mutex_b);
    return 0;
}