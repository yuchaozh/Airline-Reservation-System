#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "thread_pool.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

#define MAX_THREADS 20
#define STANDBY_SIZE 8

typedef struct pool_task_t{
    void (*function)(void *);
    void *argument;
	struct pool_task_t* next;
} pool_task_t;


struct pool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  // array of threads
  pthread_t *threads;
  // linked list for thread_pool tasks
  pool_task_t *queue;
  int thread_count;
  int task_queue_size_limit;
};

static void *thread_do_work(void *pool);


/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
    printf("create thread pool\n");
	// memory allocate for thread pool
	pool_t* threadPool = (pool_t*) malloc(sizeof(pool_t));
	threadPool->thread_count = num_threads;
	threadPool->task_queue_size_limit = queue_size;

	// create condition
	pthread_cond_t condition;
	pthread_cond_init(&condition, NULL);
	threadPool->notify = condition;
	
	// create mutex
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	threadPool->lock = mutex;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
	// initiate threads in the thread pool
	pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);
	int i;
	for (i = 0; i < num_threads; i++)
	{
		printf("create threads \n");
		int pthread_create_error = pthread_create(&threads[i], &attr, thread_do_work, (void*)threadPool);
		if (pthread_create_error)
		{
			printf("error in creating threads\n");
			exit(-1);
		}
	}
	threadPool->threads = threads;
	threadPool->queue = NULL;
	return threadPool;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, void (*function)(void *), void *argument)
{
    int err = 0;
	pthread_mutex_t* locker = &(pool->lock);
	printf("locked\n");
	err = pthread_mutex_lock(locker);
	if (err)
	{
		printf("error in locking mutex");
		return -1;
	}

	// create a new task
	pool_task_t* newTask = (pool_task_t*) malloc(sizeof(pool_task_t));
	newTask->function = function;
	newTask->argument = argument;
	newTask->next = NULL;

	// add task to the queue in thread pool
	if (!pool->queue)	//the queue is empty
		pool->queue = newTask;
	else	// the queue is not empty
	{
		pool_task_t* cursor = pool->queue;
		while (cursor->next)
		{
			cursor = cursor->next;
		}
		cursor->next = newTask;
	}

	pthread_cond_t* condition = &(pool->notify);
	err = pthread_cond_broadcast(condition);
	printf("condition broadcast\n");
	if (err)
	{
		printf("error in broadcasting condition\n");
		return -1;
	}
	err = pthread_mutex_unlock(locker);
	if (err)
	{
		printf("error in unlocking locker\n");
		return -1;
	}    
    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int err = 0;
	pool_add_task(pool, NULL, NULL);
	pthread_cond_broadcast(&(pool->notify));
	int i;
	for (i = 0; i < pool->thread_count; i++)
		pthread_join(pool->threads[i], NULL);

	free ((void*) pool->queue);
	free ((void*) pool->threads);
	free ((void*) pool);
    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *pool)
{ 
    while(1) 
	{
		pool_t* threadPool = (pool_t*) pool; 
		pthread_mutex_t* locker = &(threadPool->lock);
		pthread_cond_t* condition = &(threadPool->notify);
		
		int err = 0;
		// set lock
		err = pthread_mutex_lock(locker);
		if (err)
		{
			printf("error in locking\n");
			continue;
		}

		err = pthread_cond_wait(condition, locker);
		if (err)
			printf("error in wait\n");
		pool_task_t* cursor = threadPool->queue;
		if (!cursor)
		{
			pthread_mutex_unlock(locker);
			continue;
		}
		if (cursor->function == NULL)
		{
			pthread_mutex_unlock(&(threadPool->lock));
			pthread_exit(NULL);
			return NULL;
		}

		// delete task from linked list
		threadPool->queue = threadPool->queue->next;

		// unlock mutex for other threads
		err = pthread_mutex_unlock(locker);
		if (err)
			printf("error in unlocking");

		// do the task
		void (*function) (void*);
		void * argument;
		function = cursor->function;
		argument = cursor->argument;
		free ((void*) cursor);
		function(argument);
    }

    //pthread_exit(NULL);
    //return(NULL);
}
