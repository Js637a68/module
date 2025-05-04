#include <signal.h>
#include <event2/event.h>

void signal_cb(int fd, short event, void *arg)
{
    struct event_base *base = (struct event_base *)arg;
    struct timeval tv = {2, 0};
    printf("Caught an interrupt signal, waiting for 2 seconds...\n");
    event_base_loopexit(base, &tv);
}

void timeout_cb(int fd, short event, void *arg)
{
    printf("timeout\n");
}

int main(int argc, char **argv)
{
    struct event_base *base = event_base_new();
    struct event *signal_event = evsignal_new(base, SIGINT, signal_cb, base);
    event_add(signal_event, NULL);
    struct timeval tv = {1, 0};
    struct event *timeout_event = evtimer_new(base, timeout_cb, NULL);
    event_add(timeout_event, &tv);
    event_base_dispatch(base);
    event_free(signal_event);
    event_free(timeout_event);
    event_base_free(base);
    return 0;
}
