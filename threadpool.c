//Name: 	Shahaf Zada
//ID:		203307756
//#define TEST

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

#define SUCCESS 	0
#define ERROR 		-1
#define TRUE 		1
#define FALSE 		0
#define EMPTY		0

work_t* dequeueWork(threadpool* threadPool);
int enqueueWork(threadpool* threadPool, work_t* work);


//================Create Threadpool================
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool)
{
	#ifdef TEST
		printf("Initializing threads\n");
	#endif
	//Sanity check	if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 0)
	{
		fprintf(stderr , "Illegal num of threads\n");
		return NULL;
	}


	//Allocate threadpool memory	threadpool* result = (threadpool*)malloc(sizeof(threadpool));
	if(result == NULL)
	{
		perror("malloc");
		return NULL;
	}
	//Initialize threadpool properties
	result->num_threads = num_threads_in_pool;

	//Initialize job queue
	result->qhead = NULL;
	result->qtail = NULL;
	result->qsize = 0;

	//Initialize locks and conditions
	if(pthread_mutex_init(&result->qlock, NULL) != SUCCESS)
	{
		perror("pthread_mutex_init");
		free(result);
		return NULL;
	}

	if(pthread_cond_init(&result->q_not_empty, NULL) != SUCCESS)
	{
		perror("pthread_mutex_init");
		free(result);
		return NULL;
	}

	if(pthread_cond_init(&result->q_empty, NULL) != SUCCESS)
	{
		perror("pthread_mutex_init");
		free(result);
		return NULL;
	}

	result->shutdown = FALSE;
	result->dont_accept = FALSE;

	//Initialize threads array
	result->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads_in_pool);
	if(result->threads == NULL)
	{
		perror("malloc");
		free(result);
		return NULL;
	}
	int i;
	for(i = 0; i < num_threads_in_pool; i++)
	{
		if(pthread_create(&result->threads[i], NULL ,do_work, (void*)result) != SUCCESS)
		{
			perror("pthread_create");
			free(result->threads);
			free(result);
			return NULL;
		}
	}

	return result;}

//================Do Work================
/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p)
{
	threadpool* threadPool = (threadpool*)p;
	work_t* work;
	#ifdef TEST
		printf("new thread at your command sir!\n");
	#endif
	while(TRUE)
	{
		pthread_mutex_lock(&threadPool->qlock);

		#ifdef TEST
			printf("just minding my own buisness :P\n");
		#endif


		//Ckeck if the system is shutting down
		if(threadPool->shutdown == TRUE)
		{
			#ifdef TEST
				printf("first shutdown\n");
			#endif
			pthread_mutex_unlock(&threadPool->qlock);
			pthread_exit(SUCCESS);
		}

		//Wait for the queue to be filled (=not empty)
		if(threadPool->qsize == EMPTY)
		{
			#ifdef TEST
				printf("waiting for work\n");
			#endif

			pthread_cond_wait(&threadPool->q_not_empty, &threadPool->qlock);
		}
		#ifdef TEST
			printf("found work :/\n");
		#endif

		//Check if the system is shutting down
		if(threadPool->shutdown == TRUE)
		{
			#ifdef TEST
				printf("second shutdown\n");
			#endif
			pthread_mutex_unlock(&threadPool->qlock);
			pthread_exit(SUCCESS);
		}

		work = dequeueWork(threadPool);
		if(work == NULL)
		{
			perror("dequeue");
			return NULL;
		}
		//Check if we just took the last job, and now the queue is empty
		if(threadPool->qsize == EMPTY)
			pthread_cond_signal(&threadPool->q_empty);

		pthread_mutex_unlock(&threadPool->qlock);

		//DO YOUR GOD DAM WORK!
		work->routine(work->arg); //O_O"
		free(work);

		#ifdef TEST
			printf("Done working! :D\n");
		#endif

		/*/
		pthread_mutex_lock(&threadPool->qlock);
		//Ckeck if the system is shutting down
		if(threadPool->shutdown == TRUE)
		{
			#ifdef TEST
				printf("first shutdown\n");
			#endif
			pthread_mutex_unlock(&threadPool->qlock);
			pthread_exit(SUCCESS);
		}
		pthread_mutex_unlock(&threadPool->qlock);
		/*/
	}
}

//================Dequeue Work================
work_t* dequeueWork(threadpool* threadPool)
{
	if(threadPool == NULL || threadPool->qsize == 0)
		return NULL;

	work_t* result = threadPool->qhead;
	threadPool->qhead = threadPool->qhead->next;

	if(threadPool->qhead == NULL)	//if the queue is now empty, update tail too
		threadPool->qtail = NULL;
	threadPool->qsize--;

	#ifdef TEST
		if(threadPool->qsize == EMPTY)
			printf("Empty queue\n");
	#endif

	return result;
}

//================Enqueue Work================
int enqueueWork(threadpool* threadPool, work_t* work)
{
	if(threadPool == NULL || work == NULL)
		return ERROR;

	if(threadPool->qsize == 0)
	{	//Empty queue
		threadPool->qhead = work;
		threadPool->qtail = work;
	}
	else
	{	//Not empty queue
		threadPool->qtail->next = work;
		threadPool->qtail = work;
	}

	threadPool->qsize++;
	return SUCCESS;
}


//================Dispatch================
/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	if(from_me == NULL || dispatch_to_here == NULL)
	{
		perror("null input");
		return;
	}
	pthread_mutex_lock(&from_me->qlock);
	if(from_me->dont_accept == TRUE)
	{
		pthread_mutex_unlock(&from_me->qlock);
		return;
	}

	#ifdef TEST
		printf("Dispatching job %d\n", (int)arg);
	#endif

	work_t* work = (work_t*)malloc(sizeof(work_t));
	if(work == NULL)
	{
		perror("malloc");
		#ifdef TEST
			printf("NULL work");
		#endif
		return;
	}
	work->arg = arg;
	work->routine = dispatch_to_here;
	work->next = NULL;

	if(enqueueWork(from_me, work) == ERROR)
		perror("enqueue");

	#ifdef TEST
		printf("Signal work is ready\n");
	#endif

	pthread_cond_signal(&from_me->q_not_empty);
	pthread_mutex_unlock(&from_me->qlock);
}


//================Destroy================
/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme)
{
	if(destroyme == NULL || destroyme->threads == NULL)
		return;
	int i = 0;
	pthread_mutex_lock(&destroyme->qlock);

	//PLEASE NO MORE :(
	//Flag the dispatcher to stop accepting work
	destroyme->dont_accept = TRUE;

	#ifdef TEST
		printf("PLEASE NO MORE :(\n");
		printf("OK OK, FINISH....\n");
	#endif

	//OK OK, FINISH....
	//Let the threads finish the queue
	if(destroyme->qsize != EMPTY)
		pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);

	#ifdef TEST
		printf("WAKE UP, LAZIEASS\n");
	#endif

	//WAKE UP, LAZIEASS
	//Signal all the sleeping threads to wake up`
	destroyme->shutdown = TRUE;
	for(i = 0; i < destroyme->num_threads; i++)
		pthread_cond_signal(&destroyme->q_not_empty);
	pthread_mutex_unlock(&destroyme->qlock);


	#ifdef TEST
		printf("I've got the power!\n");
	#endif

	for(i = 0; i < destroyme->num_threads; i++)
	{
		pthread_join(destroyme->threads[i], NULL);
	}

	#ifdef TEST
		printf("*We're free!* :D\n");
	#endif

	free(destroyme->threads);
	destroyme->threads = NULL;
	free(destroyme);
}






