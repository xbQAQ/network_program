#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define DEFAULT_THREAD_ADD_SIZE 1024
#define DEFAULT_TIME 10

struct threadpool_task_t {
    void* (*function)(void* arg);
    void* arg;
}threadpool_task_t;

struct threadpool_t{
    pthread_mutex_t mutex;          //用于锁住本结构体
    pthread_mutex_t busy_mutex;     //记录忙状态线程个数的锁    --busy_thread_num

    pthread_cond_t queue_not_full;  //当任务队列满时，添加任务的线程阻塞，等待此条件变量
    pthread_cond_t queue_not_empty; //任务队列不为空时，通知等待任务的线程

    pthread_t* threads;             //存放线程池中每个线程的tid。数组
    pthread_t adjusted_tid;         //存通知线程的tid
    struct threadpool_task_t* task_queue;  //任务队列（数组首地址）

    int min_thread_num;             //线程池最小线程数
    int max_thread_num;             //线程池最大线程数
    int live_thread_num;            //线程池存活的线程数
    int busy_thread_num;            //线程池忙状态的线程数
    int wait_exit_thread_num;       //要摧毁的线程数

    int queue_front;                //队头
    int queue_rear;                 //队尾
    int queue_size;                 //任务队列中实际任务数
    int queue_max_size;             //可容纳的任务数上线

    int shutdown;                   //标志位，线程池使用状态，false或1
}threadpool_t;

struct threadpool_t* threadpool_create(int min_thread_num, int max_thread_num, int queue_max_size);
int threadpool_add(struct threadpool_t* pool, void*(*function)(void*), void* arg);
void* threadpool_thread(void* threadpool);
void* adjust_thread(void* threadpool);
int is_thread_alive(pthread_t tid);
int threadpool_destroy(struct threadpool_t *pool);
int threadpool_free(struct threadpool_t* pool);
void* process(void* arg);

struct threadpool_t* threadpool_create(int min_thread_num, int max_thread_num, int queue_max_size){
    int i;
    struct threadpool_t* pool = NULL;
    do{
        if((pool = (struct threadpool_t*)malloc(sizeof(struct threadpool_t))) == NULL){
            printf("malloc failed\n");
            break;
        }

        pool->min_thread_num = min_thread_num;
        pool->max_thread_num = max_thread_num;
        pool->live_thread_num = min_thread_num;
        pool->busy_thread_num = 0;
        pool->wait_exit_thread_num = 0;
        pool->queue_front = 0;
        pool->queue_rear = 0;
        pool->queue_size = 0;
        pool->queue_max_size = queue_max_size;
        pool->shutdown = 0;

        /*根据最大线程上限数，给工作线程数组开辟空间，并清零*/
        pool->threads = (pthread_t*)malloc(pool->max_thread_num * sizeof(pthread_t));
        if(pool->threads == NULL){
            printf("malloc threads failed\n");
            break;
        }
        memset(pool->threads, 0, sizeof(pthread_t) * pool->max_thread_num);

        /*给任务队列开辟空间*/
        pool->task_queue = (struct task_queue *)malloc(sizeof(struct threadpool_task_t) * queue_max_size);
        if(pool->task_queue == NULL){
            printf("malloc task queue failed\n");
            break;
        }

        /*初始化互斥锁，条件变量*/
        if((pthread_mutex_init(&pool->mutex, NULL) != 0) || (pthread_mutex_init(&pool->busy_mutex, NULL) != 0)
        || (pthread_cond_init(&pool->queue_not_full, NULL) != 0) || (pthread_cond_init(&pool->queue_not_empty, NULL) != 0)){
            printf("initialized the lock and condition variable error\n");
            break;
        } 

        /*启动 min_thread_num 个工作线程*/
        for(int i = 0; i != min_thread_num; i++){
            pthread_create(&pool->threads[i], NULL, threadpool_thread, (void*)pool);
            printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
        }
        /*创建管理者线程*/
        pthread_create(&pool->adjusted_tid, NULL, adjust_thread, (void*)pool);
        
        return pool;
    }while(0);

    threadpool_free(pool);  //前面代码调用失败，释放pool存储空间

    return NULL;
}

/*向线程池加一个任务*/
int threadpool_add(struct threadpool_t* pool, void*(*function)(void*), void* arg){
    pthread_mutex_lock(&pool->mutex);

    /* 队列已满，调wait阻塞*/
    while((pool->queue_size == pool->queue_max_size) && (!pool->shutdown)){
        pthread_cond_wait(&pool->queue_not_full, &pool->mutex);
    }

    //关闭线程池
    if(pool->shutdown){
        pthread_cond_broadcast(&pool->queue_not_empty);
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }

    /*清空工作线程调用的回调函数的参数arg*/
    if(pool->task_queue[pool->queue_rear].arg != NULL) {
        pool->task_queue[pool->queue_rear].arg = NULL; 
    }

    /*添加任务到任务队列里*/
    if(pool->queue_rear + 1 != pool->queue_front){
        pool->task_queue[pool->queue_rear].function = function;
        pool->task_queue[pool->queue_rear].arg = arg;
        pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;
        pool->queue_size++;
    }

    /*添加完任务后，队列不为空，唤醒线程池中等待任务队列的线程*/
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

//线程池里线程的工作
void* threadpool_thread(void* threadpool){
    struct threadpool_t* pool = (struct threadpool_t*)threadpool;
    struct threadpool_task_t task;

    while(1){
        /*刚创建出线程，等待任务队列里有任务，否则阻塞等待任务队列里有任务后再唤醒任务*/
        pthread_mutex_lock(&pool->mutex);
        /*pool->queue_size == 0说明没有任务，调wait阻塞在条件变量上，若有任务，跳过该while*/
        while((pool->queue_size == 0) && (!pool->shutdown)) {
            printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&pool->queue_not_empty, &pool->mutex);

            /*清除指定数目的空闲线程，若要结束的线程个数大于0，结束线程*/
            if(pool->wait_exit_thread_num > 0){
                pool->wait_exit_thread_num--;

                //如果线程池活着的线程比最小线程数还多，那么结束当前线程
                if(pool->live_thread_num > pool->min_thread_num){
                    printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                    pool->live_thread_num--;
                    pthread_mutex_unlock(&pool->mutex);

                    pthread_exit(NULL);
                }
            }
        }

        /*如果指定了1，要关闭线程池里每个线程，自行退出处理--销毁线程池*/
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutex);
            printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
            pthread_detach(pthread_self());     //分离线程
            pthread_exit(NULL);     //线程自行结束
        }

        /*从任务队列里获取任务，是一个出队任务*/
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;

        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
        pool->queue_size--;     //任务数减一

        /*通知可以加任务进来*/
        pthread_cond_broadcast(&pool->queue_not_full);

        /*任务取出后，立即将线程池解锁*/
        pthread_mutex_unlock(&pool->mutex);

        /*执行任务*/
        printf("thread 0X%x start working\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&pool->busy_mutex);
        pool->busy_thread_num++;
        pthread_mutex_unlock(&pool->busy_mutex);

        task.function(task.arg);        //执行任务
        
        /*任务结束处理*/
        printf("thread 0X%x finish working\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&pool->busy_mutex);
        pool->busy_thread_num--;
        pthread_mutex_unlock(&pool->busy_mutex);
    }

    pthread_exit(NULL);
}

/*管理线程*/
void* adjust_thread(void* threadpool){
    int i;
    struct threadpool_t* pool = (struct threadpool_t*)threadpool;

    //线程池没有关闭
    while(!pool->shutdown){
        sleep(DEFAULT_TIME);      //定期对线程池管理

        pthread_mutex_lock(&pool->mutex);
        int queue_size = pool->queue_size;
        int live_thread_num = pool->live_thread_num;
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&pool->busy_mutex);
        int busy_thread_num = pool->busy_thread_num;
        pthread_mutex_unlock(&pool->busy_mutex);

        /*创建新线程: 算法：任务数大于最小线程数，并且存货线程数小于最大线程数*/
        if(queue_size > pool->min_thread_num && live_thread_num < pool->max_thread_num){
            pthread_mutex_lock(&pool->mutex);
            int add = 0;
            
            /*一次增加 DEFAULT_THREAD_ADD_SIZE个线程*/
            for(int i = 0; i != pool->max_thread_num && add < DEFAULT_THREAD_ADD_SIZE 
                && pool->live_thread_num < pool->max_thread_num; i++){
                if(pool->threads[i] == 0 || !is_thread_alive(pool->threads[i])){
                    pthread_create(&pool->threads[i], NULL, threadpool_thread, (void *)pool);
                    add++;
                    pool->live_thread_num++;
                }
            }

            pthread_mutex_unlock(&pool->mutex);
        }

        /*摧毁多余的空闲线程 算法：忙线程 × 2 < 存活的线程数 且 存活的线程数 > 最小线程数*/
        if(busy_thread_num * 2 < live_thread_num && live_thread_num > pool->min_thread_num){
            /*一次摧毁DEFAULT_THREAD_ADD_SIZE个线程，随机10个即可*/
            pthread_mutex_lock(&pool->mutex);
            pool->wait_exit_thread_num = DEFAULT_THREAD_ADD_SIZE;
            pthread_mutex_unlock(&pool->mutex);

            for(int i = 0; i != DEFAULT_THREAD_ADD_SIZE; i++){
                pthread_cond_signal(&pool->queue_not_empty);
            }
        }
    }
}

/*线程是否存活*/
int is_thread_alive(pthread_t tid)
{
   int kill_rc = pthread_kill(tid, 0);     //发送0号信号，测试是否存活
   if (kill_rc == ESRCH){  //线程不存在
      return -1;
   }
   return 0;
}

int threadpool_destroy(struct threadpool_t *pool){
    int i;
    if(pool == NULL){
        return -1;
    }

    pool->shutdown = 1;

    /*先摧毁管理线程*/
    pthread_join(pool->adjusted_tid, NULL);

    for(int i = 0; i != pool->live_thread_num; i++){
        /*通知所有的空闲线程*/
        pthread_cond_signal(&pool->queue_not_empty);
    }

    for(int i = 0; i != pool->live_thread_num; i++){
        pthread_join(pool->threads[i], NULL);
    }
    threadpool_free(pool);

    return 0;
}

int threadpool_free(struct threadpool_t* pool){
    if(pool == NULL) return -1;
    if(pool->task_queue){
        free(pool->task_queue);
    }
    if(pool->threads){
        free(pool->threads);
        pthread_mutex_lock(&pool->mutex);       //先锁住再摧毁
        pthread_mutex_destroy(&pool->mutex);
        pthread_mutex_lock(&pool->busy_mutex);
        pthread_mutex_destroy(&pool->busy_mutex);
        pthread_cond_destroy(&pool->queue_not_empty);
        pthread_cond_destroy(&pool->queue_not_full);
    }
    free(pool);
    pool = NULL;    

    return 0;
}

//模拟处理业务
void* process(void* arg){
    printf("thread 0x%x working on task %d\n", (unsigned int)pthread_self(), *(int*)arg);
    sleep(1);   //小写转大写
    printf("task %d is end\n", *(int*)arg);

    return NULL; 
}

int main(){
    struct threadpool_t* pool = threadpool_create(3, 100, 100);
    printf("pool initialized\n");

    int num[20];
    for(int i = 0; i != 20; i++){
        num[i] = i;
        printf("add task%d\n", i);

        threadpool_add(pool, process, (void*)&num[i]);
    }

    sleep(10);
    threadpool_destroy(pool);

    return 0;   
}