#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_key_t key;

void echomsg(void *t) {
    printf("destructor executed in thread %d, param=%d\n", pthread_self(), (int)t);
}

void *child1(void *arg) {
    int tid = pthread_self();
    printf("thread %d enter\n", tid);
    pthread_setspecific(key, (void *)tid);
    sleep(2);
    printf("thread %d return %d\n", tid, pthread_getspecific(key));
    sleep(5);
    return 0;
}

void *child2(void *arg) {
    int tid = pthread_self();
    printf("thread %d enter\n", tid);
    pthread_setspecific(key, (void *)tid);
    sleep(1);
    printf("thread %d return %d\n", tid, pthread_getspecific(key));
    sleep(5);
    return 0;
}

int main(int argc, char *argv[]) {
    pthread_t tid1, tid2;
    printf("hello\n");
    pthread_key_create(&key, echomsg);
    pthread_create(&tid1, NULL, child1, NULL);
    pthread_create(&tid2, NULL, child2, NULL);
    sleep(10);
    pthread_key_delete(key);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    printf("main thread exit\n");
    return 0;
}
