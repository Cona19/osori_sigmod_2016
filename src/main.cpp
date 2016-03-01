#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <map>
#include <queue>
#include <pthread.h>
#include "mvgraph.h"
#include "config.h"

#define RESULT_POOL_MAX		100000

MVGraph* mvGraph;

/*
	for Thread pool
*/
typedef struct _Thd
{
	int					tid_;
	int					version_;
	int					startNodeNum_;
	int					endNodeNum_;

	pthread_mutex_t		mutex_;
	pthread_cond_t		condVar_;
} Thd;

pthread_cond_t		async_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t		async_mutex = PTHREAD_MUTEX_INITIALIZER;

Thd					thdData[NUM_THREAD];

int					thdPool[NUM_THREAD];
int					thdPoolIter;

/*
   for pooling result
*/
int					batchVersion;
int					resultPool[RESULT_POOL_MAX];
int					cntQinBatch;
int					cntResult;

void* thread_func(void* data);

int main()
{
	// input initial graph until signal 'S' received
	char str[16];
	unsigned int node1, node2;

	// create thread pool
	Thd t;
	pthread_t pthread;
	for( int i = 0; i < NUM_THREAD; i++ )
	{
		thdData[i].tid_ = i;
		thdData[i].mutex_ = PTHREAD_MUTEX_INITIALIZER;
		thdData[i].condVar_ = PTHREAD_COND_INITIALIZER;

		//thdPool.insert( pair<int, int>(0, i) );
		thdPool[i] = 1;

		pthread_mutex_lock( &async_mutex );
		if( pthread_create( &pthread, NULL, thread_func, (void*)&i ) < 0 )
		{
			std::cerr << "failed to create thread pool" << std::endl;
			return 0;
		}
		pthread_cond_wait( &async_cond, &async_mutex );
		pthread_mutex_unlock( &async_mutex );
	}

	// create graph with initial data
	mvGraph = new MVGraph();
	//FILE* fp = fopen("input.txt", "r");
	while( 1 )
	{
		scanf("%s", str);
		//fscanf(fp, "%s", str);
		if( str[0] == 'S' )
			break;

		node1 = atoi(str);

		scanf("%s", str);
		//fscanf(fp, "%s", str);
		node2 = atoi(str);

		mvGraph->addEdge( node1, node2, 0 ); 
	}

	int version = 0;
	batchVersion = 0;
	cntResult = 0;
	cntQinBatch = 0;

	std::cout << "R" << std::endl;
	char queryType;

	while( 1 )
	{
		queryType = 'Z';
		scanf("%c", &queryType);
		//fscanf(fp, "%c", &queryType);
		
		if( queryType == 'Q' )
		{
			scanf("%d %d", &node1, &node2);
			//fscanf(fp, "%d %d", &node1, &node2);

			version++;
			cntQinBatch++;
			while( 1 )
			{
				int cntIter = 0;
				while( thdPool[thdPoolIter] == 1 )
				{
					thdPoolIter = (thdPoolIter + 1) % NUM_THREAD;
					cntIter++;

					if( cntIter == NUM_THREAD )
					{
						cntIter = 0;
						sched_yield();
					}
				}

				thdData[thdPoolIter].version_ = version;
				thdData[thdPoolIter].startNodeNum_ = node1;
				thdData[thdPoolIter].endNodeNum_ = node2;
				thdPool[thdPoolIter] = 1;

				// lock thread's mutex to prevent a situation that
				// main thread wake a thread up whose state is 0
				// but not sleep yet	
				pthread_mutex_lock( &thdData[thdPoolIter].mutex_ );
				pthread_cond_signal( &thdData[thdPoolIter].condVar_ );
				pthread_mutex_unlock( &thdData[thdPoolIter].mutex_ );

				break;
			}
		}
		else if( queryType == 'A' )
		{
			scanf("%d %d", &node1, &node2);
			//fscanf(fp, "%d %d", &node1, &node2);
			mvGraph->addEdge( node1, node2, version ); 
		}
		else if( queryType == 'D' )
		{
			scanf("%d %d", &node1, &node2);
			//fscanf(fp, "%d %d", &node1, &node2);
			mvGraph->removeEdge( node1, node2, version );
		}
		else if( queryType == 'F' )
		{
			// print results in this batch
			while( cntQinBatch != cntResult )
			{
				sched_yield();	
			}

			for( int i = 0; i < cntQinBatch; i++ )
			{
				std::cout << resultPool[i] << std::endl;
			}
			
			scanf(" ");
			//fscanf(fp, " ");
			if( (float)mvGraph->cntRemovedEdge_ / (float)mvGraph->cntEdge_
					> 0.2f )
			{
				// garbage collect
				std::cerr << "run GC" << std::endl;
				mvGraph->garbageCollect();
			}

			batchVersion = version;
			cntQinBatch = 0;
			cntResult = 0;

		}
		else if( queryType == 'Z' )
		{
			break;
		}
	}

	return 0;
}

void* thread_func(void* data)
{
	int tid = *((int*)data);
	pair<Node*, int>* q = new pair<Node*, int>[QUEUE_MAX];
	
	multimap<int, int>::iterator mi;

	int s, e, version;

	pthread_mutex_lock( &async_mutex );
	pthread_cond_signal( &async_cond );
	pthread_mutex_unlock( &async_mutex );

	while( 1 )
	{
		// lock thread's mutex to prevent a situation that
		// main thread wake a thread up whose state is 0
		// but not sleep yet
		pthread_mutex_lock( &thdData[tid].mutex_ );
		thdPool[tid] = 0;
		pthread_cond_wait( &thdData[tid].condVar_, &thdData[tid].mutex_ );
		pthread_mutex_unlock( &thdData[tid].mutex_ );

		s = thdData[tid].startNodeNum_;
		e = thdData[tid].endNodeNum_;
		version = thdData[tid].version_;

		int result = mvGraph->findSPP(s, e, version, tid, q);
		resultPool[version - batchVersion - 1] = result;

		__sync_fetch_and_add( &cntResult, 1 );
	}
}

