#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "common.h"
#include "clustering.h"

#define TEST_WITH_FILE

map<nid_t, Node> nodes;
map<cid_t, Cluster> clusters;
vector<Edge> edges;

int numberOfCores;
vid_t globVer;

typedef struct _Command
{
	char type;
	nid_t startNodeID;
	nid_t endNodeID;

	vid_t ver;

	cid_t clusterID;				// only for update command
} Command;

map< cid_t, list<Command> > groupedUpdateCmdList;
list<Command> queryCmdList;

// thread 관리
// 일을 마친 thread는 mutex를 잡고 cntSleepThread를 1 올리고 condWork로 잔다
// 메인 thread는 cntSleepThread값을 쳐다보다가 모두 일을 마쳤는지 판단하고
// Phase를 전환한다.
// condWork로 일괄적으로 깨어난 thread는 현재 phase를 확인하여
// updateCmdList 또는 queryCmdList를 보며 일한다.
pthread_cond_t condInit;			// thread pool 생성용
pthread_mutex_t mutexInit;			// thread pool 생성용
pthread_cond_t condWork;
pthread_mutex_t mutexWork;
pthread_t* threads;
volatile int cntSleepThread;		// 일을 마치고 잠든 thread 수

enum Phase
{
	PhaseInit,
	PhaseUpdate,
	PhaseQuery
};
Phase phase;

void initThreads();
void updatePhase();
void queryPhase();

void* executeCommand(void* tid);

void printCluster();

#ifdef TEST_WITH_FILE
int main(int argc, char** argv)
#else
int main()
#endif
{
	numberOfCores = sysconf( _SC_NPROCESSORS_ONLN );

	nid_t node1, node2;

	char str[32];
#ifdef TEST_WITH_FILE
	FILE* fp = fopen( argv[1], "r" );
#endif
	while( 1 )
	{
#ifdef TEST_WITH_FILE
		fscanf(fp, "%s", str);
#else
		scanf("%s", str);
#endif
		if( str[0] == 'S' )
			break;

		node1 = atoi(str);

#ifdef TEST_WITH_FILE		
		fscanf(fp, "%s", str);
#else
		scanf("%s", str);
#endif
		node2 = atoi(str);

		Edge newEdge;
		newEdge.score = 0;
		edges.push_back( newEdge );

		// edge의 시작 노드 찾기
		map<nid_t, Node>::iterator it = nodes.find( node1 );
		if( it == nodes.end() )
		{
			// 새로운 노드가 추가된 경우
			Node newNode;
			newNode.clusterID = -1; 
			newNode.nid = node1;
			newNode.inOutEdges.insert( node2 );
			pair< map<nid_t, Node>::iterator, bool > ret =
				nodes.insert( map<nid_t, Node>::
						value_type( node1, newNode ) );
			it = ret.first;
		}   
		else
		{
			it->second.inOutEdges.insert( node2 );
		}
		// edge의 도착 노드 찾기
		map<nid_t, Node>::iterator it2 = nodes.find( node2 );
		if( it2 == nodes.end() )
		{
			// 새로운 노드가 추가된 경우
			Node newNode;
			newNode.clusterID = -1;
			newNode.nid = node2;
			newNode.inOutEdges.insert( node1 );
			pair< map<nid_t, Node>::iterator, bool > ret =
				nodes.insert( map<nid_t, Node>::
						value_type( node2, newNode ) );
			it2 = ret.first;
		}
		else
		{
			it2->second.inOutEdges.insert( node1 );
		}

		it->second.outEdges.insert( &(it2->second) );
		it2->second.inEdges.insert( &(it->second) );

		edges[edges.size()-1].start = &(it->second);
		edges[edges.size()-1].end = &(it2->second);
	}

	// clustering하고 나면 clusters 데이터 생성되고, 각 node의 inEdges와 outEdges는 동일한 cluster의 edge만 남게 된다
	clustering();

	edges.clear();
#ifdef TEST_WITH_FILE
	fclose(fp);
#endif

	// 일꾼들 초기화
	initThreads();

	cout << "R" << endl;

	double elapsedTime;
	timeval startTime, endTime; 
	gettimeofday( &startTime, NULL );

	char queryType;
#ifdef TEST_WITH_FILE
	fp = fopen( argv[2], "r" );
#endif

	list<Command> commandList;
	list<Command> updateCmdList;
	globVer = 1;
	while( 1 )
	{
		queryType = 'Z';
#ifdef TEST_WITH_FILE
		fscanf(fp, "%c", &queryType);
#else
		scanf("%c", &queryType);
#endif

		if( queryType == 'Q' || queryType == 'A' || queryType == 'D' )
		{
			// 한 batch의 모든 command를 쭉 받는다.
			// 모두 받은 뒤, update와 Q를 분리한다.
#ifdef TEST_WITH_FILE
			fscanf(fp, "%d %d", &node1, &node2);
#else
			scanf("%d %d", &node1, &node2);
#endif

			// Q가 들어올 때마다 version을 증가시킨다.
			// Q는 자기 version보다 작은 version의 data를 사용해야 한다.
			if( queryType == 'Q' )
				globVer++;

			Command cmd;
			cmd.type = queryType;
			cmd.startNodeID = node1;
			cmd.endNodeID = node2;
			cmd.ver = globVer;

			commandList.push_back( cmd );
		}
		else if( queryType == 'F' )
		{
			timeval t1, t2; 
			gettimeofday( &t1, NULL );

			updateCmdList.clear();
			queryCmdList.clear();

			for( list<Command>::iterator it = commandList.begin();
										 it != commandList.end(); it++ )
			{
				if( it->type == 'Q' )
					queryCmdList.push_back( *it );
				else
					updateCmdList.push_back( *it );
			}
			commandList.clear();

			// UPDATE command를 한번 쭉 훑으며,
			// ADD의 경우 추가되는 노드는 추가해주고, 동일한 cluster 내의 ADD/DELETE인지를 판단해서 동일한 경우 해당 clusterID를 기록해두고, 다른 경우 Bridge를 즉시 추가/제거한다.
			list<Command>::iterator it = updateCmdList.begin();
			while( it != updateCmdList.end() )
			{
				if( it->type == 'A' )
				{
					map<nid_t, Node>::iterator nit1 =
					   	nodes.find( it->startNodeID );
					map<nid_t, Node>::iterator nit2 =
						nodes.find( it->endNodeID );
	
					if( nit1 == nodes.end() )
					{
						if( nit2 == nodes.end() )
						{
							// 둘 다 새로운 노드
							// random한 cluster에 두 노드 배치
							int r = rand() % clusters.size();
							r++;
	
							Node newNode1;
							newNode1.clusterID = r;
							newNode1.nid = it->startNodeID;
							newNode1.cluster = &(clusters[r]);
							
							Node newNode2;
							newNode2.clusterID = r;
							newNode2.nid = it->endNodeID;
							newNode2.cluster = newNode1.cluster;
	
							nodes.insert( pair<nid_t, Node>(
									it->startNodeID, newNode1 ) );
	
							nodes.insert( pair<nid_t, Node>(
									it->endNodeID, newNode2 ) );
	
							it->clusterID = r;
						}
						else
						{
							// 시작노드가 새로운 노드
							Node newNode;
							newNode.clusterID = nit2->second.clusterID;
							newNode.nid = it->startNodeID;
							newNode.cluster = nit2->second.cluster;
		
							nodes.insert( map<nid_t, Node>::value_type(
									it->startNodeID, newNode ) );

							it->clusterID = newNode.clusterID;
						}
						it++;
					}
					else if( nit2 == nodes.end() )
					{
						// 도착노드가 새로운 노드
						Node newNode;
						newNode.clusterID = nit1->second.clusterID;
						newNode.nid = it->endNodeID;
						newNode.cluster = nit1->second.cluster;

						nodes.insert( map<nid_t, Node>::value_type(
								it->endNodeID, newNode ) );

						it->clusterID = newNode.clusterID;
						it++;
					}
					else
					{
						// 두 노드 모두 존재
						// 두 노드가 같은 cluster면 clusterID만 기록, 아니면 Bridge 추가 후 command 제거
						if( nit1->second.clusterID ==
							nit2->second.clusterID )
						{
							it->clusterID = nit1->second.clusterID;
							it++;
							continue;
						}
						// Bridge 추가 후 command 제거
						// 시작 노드의 cluster에 out bridge 추가
						// 도착 노드의 cluster에 in bridge 추가
						// 이미 있는 bridge라면 ValidNode만 달아주고,
						// 없는 bridge라면 새로 추가
						bool bExist = false;
						for( list<Bridge>::iterator it_b =
							 nit1->second.cluster->outBridges.begin();
							 it_b != nit1->second.cluster->outBridges.end();
							 it_b++ )
						{
							if( it_b->src == &(nit1->second) &&
								it_b->dest == &(nit2->second) )
							{
								// 이미 out bridge 존재
								ValidNode newValidNode;
								newValidNode.ver = it->ver;
								newValidNode.isValid = true;
								it_b->validList.push_front( newValidNode );
								bExist = true;
								break;
							}
						}
						if( !bExist )
						{
							// 새로운 Bridge 추가
							Bridge newBridge;
							newBridge.src = &(nit1->second);
							newBridge.dest = &(nit2->second);
							ValidNode newValidNode;
							newValidNode.ver = it->ver;
							newValidNode.isValid = true;
							newBridge.validList.push_front( newValidNode );

							nit1->second.cluster->outBridges.push_front(
									newBridge );
							nit2->second.cluster->inBridges.push_front(
									newBridge);
							it = updateCmdList.erase( it );
							continue;
						}
						// out bridge가 존재하면 반대쪽 cluster의 in bridge도 무조건 존재
						for( list<Bridge>::iterator it_b =
							 nit2->second.cluster->inBridges.begin();
							 it_b != nit2->second.cluster->inBridges.end();
							 it_b++ )
						{
							if( it_b->src == &(nit1->second) &&
								it_b->dest == &(nit2->second) )
							{
								ValidNode newValidNode;
								newValidNode.ver = it->ver;
								newValidNode.isValid = true;
								it_b->validList.push_front( newValidNode );
								break;
							}
						}

						it = updateCmdList.erase( it );
					}
				}
				else // D
				{
					map<nid_t, Node>::iterator nit1 =
						nodes.find( it->startNodeID );
					if( nit1 == nodes.end() )
					{
						// 없는 노드의 edge를 지우려고 함 - command 제거
						it = updateCmdList.erase( it );
					}
					else
					{
						map<nid_t, Node>::iterator nit2 =
							nodes.find( it->endNodeID );
						if( nit2 == nodes.end() )
						{
							// 없는 노드의 edge를 지우려고 함 - command 제거
							it = updateCmdList.erase( it );
							continue;
						}

						// 두 노드의 cluster가 같으면 clusterID 기록, 다르면 Bridge 제거 후 command 제거
						if( nit1->second.clusterID ==
								nit2->second.clusterID )
						{
							it->clusterID = nit1->second.clusterID;
							it++;
							continue;
						}

						// 두 노드의 cluster가 다르면
						// Bridge 제거 후 command 제거
						// 시작 노드의 cluster에 out bridge 제거
						// 도착 노드의 cluster에 in bridge 제거
						// 이미 있는 bridge라면 ValidNode만 달아주고,
						// 없는 bridge라면 무시(이경우는 없음)
						bool bExist = false;
						for( list<Bridge>::iterator it_b =
							 nit1->second.cluster->outBridges.begin();
							 it_b != nit1->second.cluster->outBridges.end();
							 it_b++ )
						{
							if( it_b->src == &(nit1->second) &&
								it_b->dest == &(nit2->second) )
							{
								// 이미 out bridge 존재
								ValidNode newValidNode;
								newValidNode.ver = it->ver;
								newValidNode.isValid = false;
								it_b->validList.push_front( newValidNode );
								bExist = true;
								break;
							}
						}
						if( !bExist )
						{
							it = updateCmdList.erase( it );
							continue;
						}
						// out bridge가 존재하면 반대쪽 cluster의 in bridge도 무조건 존재
						for( list<Bridge>::iterator it_b =
							 nit2->second.cluster->inBridges.begin();
							 it_b != nit2->second.cluster->inBridges.end();
							 it_b++ )
						{
							if( it_b->src == &(nit1->second) &&
								it_b->dest == &(nit2->second) )
							{
								ValidNode newValidNode;
								newValidNode.ver = it->ver;
								newValidNode.isValid = false;
								it_b->validList.push_front( newValidNode );
								break;
							}
						}
							
						it = updateCmdList.erase( it );
					}
				}
			}

			// updateCmdList를 cluster별로 저장
			groupedUpdateCmdList.clear();
			for( list<Command>::iterator it = updateCmdList.begin();
										 it != updateCmdList.end(); it++ )
			{
				map< cid_t, list<Command> >::iterator it2;
				it2 = groupedUpdateCmdList.find( it->clusterID );
				if( it2	== groupedUpdateCmdList.end() )
				{
					pair< cid_t, list<Command> > cmd;
					cmd.first = it->clusterID;
					list<Command> tmpList;
					tmpList.push_back( *it );
					cmd.second = tmpList;
					groupedUpdateCmdList.insert( cmd );
				}
				else
				{
					it2->second.push_back( *it );
				}
			}

			gettimeofday( &t2, NULL );
			double t = (t2.tv_sec - t1.tv_sec) * 1000.0;
			t += (t2.tv_usec - t1.tv_usec) / 1000.0;
			elapsedTime += t;

			updatePhase();
			queryPhase();
		}
		else if( queryType == 'Z' )
		{
			break;
		}
	}

	gettimeofday( &endTime, NULL );
	double t = (endTime.tv_sec - startTime.tv_sec) * 1000.0;
	t += (endTime.tv_usec - startTime.tv_usec) / 1000.0;
	printf("Ftime: %lf, time: %lf\n", elapsedTime, t);
}

void initThreads()
{
	phase = PhaseInit;
	cntSleepThread = 0;

	threads = new pthread_t[numberOfCores-1];

	for( long i = 0; i < numberOfCores-1; i++ )
	{
		pthread_mutex_lock( &mutexInit );
		if( pthread_create( &threads[i], NULL, executeCommand, (void*)i )
				< 0 )
		{
			return;
		}
		pthread_cond_wait( &condInit, &mutexInit );
		pthread_mutex_unlock( &mutexInit );
	}

	// 생성한 thread들이 모두 cntSleepThread를 올릴 때 까지 기다림
	while( cntSleepThread < numberOfCores-1 )
		sched_yield();
}

void updatePhase()
{
	phase = PhaseUpdate;

	cntSleepThread = 0;

	pthread_mutex_lock( &mutexWork );
	pthread_cond_broadcast( &condWork );
	pthread_mutex_unlock( &mutexWork );

	while( cntSleepThread < numberOfCores-1 )
		sched_yield();
}

void queryPhase()
{
	phase = PhaseQuery;

	cntSleepThread = 0;

	pthread_mutex_lock( &mutexWork );
	pthread_cond_broadcast( &condWork );
	pthread_mutex_unlock( &mutexWork );

	while( cntSleepThread < numberOfCores-1 )
		sched_yield();

	// cluster에 처리중 마크 해제
	for( map< cid_t, list<Command> >::iterator
								 it = groupedUpdateCmdList.begin();
								 it != groupedUpdateCmdList.end(); it++ )
	{
		clusters[it->first].isUpdating = false;
	}
}

void* executeCommand(void* tid)
{
	long threadID = (long)tid;

	pthread_mutex_lock( &mutexInit );
	pthread_cond_signal( &condInit );
	pthread_mutex_unlock( &mutexInit );

	while( 1 )
	{
		pthread_mutex_lock( &mutexWork );
		cntSleepThread++;
		pthread_cond_wait( &condWork, &mutexWork );
		pthread_mutex_unlock( &mutexWork );

		if( phase == PhaseUpdate )
		{
			// update command list를 한번 쭉 순회하며,
			// 처리할 수 있는 것들을 처리하면서 내려온 뒤 자면 된다.
			// 중간에 처리중인 cluster에 대해 그냥 지나온 것들은
			// 해당 cluster를 처리했던 thread가 처리하고 온다.
			for( map< cid_t, list<Command> >::iterator
										 it = groupedUpdateCmdList.begin();
										 it != groupedUpdateCmdList.end();
										 it++ )
			{
				bool doWork = __sync_bool_compare_and_swap(
									&clusters[it->first].isUpdating,
									false,
									true );
				if( doWork )
				{
					// 동일한 cluster에 대한 update는 sequential하게 실행
					// 해야 하므로, 동일한 thread가 쭉 돈다.
					for( list<Command>::iterator it2 = it->second.begin();
												 it2 != it->second.end();
												 it2++ )
					{
						if( it2->type == 'A' )
						{
							// TODO: 갓현석님의 ADD 시전
							Node* start = &(nodes[it2->startNodeID]);
							Node* end = &(nodes[it2->endNodeID]);
							int ver = it2->ver;

//							printf("A %d %d, ver:%d tid:%d\n", it2->startNodeID, it2->endNodeID, ver, threadID);
						}
						else
						{
							// TODO: 갓현석님의 DELETE 시전
							Node* start = &(nodes[it2->startNodeID]);
							Node* end = &(nodes[it2->endNodeID]);
							int ver = it2->ver;

//							printf("D %d %d, ver:%d tid:%d\n", it2->startNodeID, it2->endNodeID, ver, threadID);
						}
					}
				}
			}
		}
		else if( phase == PhaseQuery )
		{
			int i = 0;
			for( list<Command>::iterator it = queryCmdList.begin();
										 it != queryCmdList.end(); it++ )
			{
				if( (i+(numberOfCores-1)-threadID) % (numberOfCores-1)
						== 0 )
				{
					// TODO: 갓희원님의 길찾기 시전
					// 없는 node는 패스
					map<nid_t, Node>::iterator itNode1 =
					   	nodes.find( it->startNodeID );
					if( itNode1 == nodes.end() )
					{
						i++;
						continue;
					}

					map<nid_t, Node>::iterator itNode2 =
						nodes.find( it->endNodeID );
					if( itNode2 == nodes.end() )
					{
						i++;
						continue;
					}

					Node* start = &(itNode1->second);
					Node* end = &(itNode2->second);
					int ver = it->ver;

//					printf("Q %d %d, ver:%d tid:%d\n", it->startNodeID, it->endNodeID, ver, threadID);
				}
				i++;
			}

		}
	}
}

void printCluster()
{
	for( map<nid_t, Node>::iterator it = nodes.begin(); it != nodes.end();
									it++ )
	{
		printf("node %d : cluster %d\n", it->second.nid, it->second.clusterID );
	}
	for( map<cid_t, Cluster>::iterator it = clusters.begin();
									   it != clusters.end(); it++ )
	{
		printf("---- cluster %d ----\n", it->first);
		printf("--- inBridges ---\n");
		for( list<Bridge>::iterator it2 = it->second.inBridges.begin();
									it2 != it->second.inBridges.end();
									it2++ )
		{
			printf("--[%d->%d]--\n", it2->src->nid, it2->dest->nid);
			for( list<ValidNode>::iterator it3 = it2->validList.begin();
										   it3 != it2->validList.end(); it3++ )
			{
				printf("ver:%d isValid:%d\n", it3->ver, (int)it3->isValid);
			}
			printf("\n");
		}
		printf("\n");
		printf("--- outBridges ---\n");
		for( list<Bridge>::iterator it2 = it->second.outBridges.begin();
									it2 != it->second.outBridges.end();
									it2++ )
		{
			printf("--[%d->%d]--\n", it2->src->nid, it2->dest->nid);
			for( list<ValidNode>::iterator it3 = it2->validList.begin();
										   it3 != it2->validList.end(); it3++ )
			{
				printf("ver:%d isValid:%d\n", it3->ver, (int)it3->isValid);
			}
			printf("\n");
		}
	}
}

