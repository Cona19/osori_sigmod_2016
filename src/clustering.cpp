#include "clustering.h"
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <iostream>
#include <set>
#include <map>
#include <algorithm>
#include <queue>

#define SWAP(a, b) (a ^= b ^= a ^= b)

extern boost::unordered_map<nid_t, Node> nodes;
extern map<cid_t, Cluster> clusters;
extern vector<Edge> edges;
extern int numberOfCores;

void* calcEdgeScore(void* tid);
void* calcLSP(void* tid);

bool compEdge(Edge first, Edge second);
bool compSubgraph(vector<nid_t> first, vector<nid_t> second);
bool compClusterSize(vector<Node*> first, vector<Node*> second);

vector<int> preClustering(int maxClusterSize);

void clustering()
{
	timeval startTime, endTime; 
	gettimeofday( &startTime, NULL );

	// subgraph를 모두 찾은 뒤, 작은 subgraph끼리 뭉쳐서 cluster를 만든다
	int rootN = sqrt( nodes.size() );
	int maxClusterNum = rootN;
	int maxClusterSize = rootN;

	vector<int> preCluster = preClustering( maxClusterSize );
	int cntPreCluster = preCluster.size();
	int cntPreClusteredNode = 0;
	for( int i = 0; i < preCluster.size(); i++ )
	{
		cntPreClusteredNode += preCluster[i];
	}

	// multi thread로 각 edge의 score 매기기 
	pthread_t* pthread = new pthread_t[numberOfCores];
	for( long i = 0; i < numberOfCores; i++ )
	{
		if( pthread_create( &pthread[i], NULL, calcEdgeScore, (void*)i )
			   	< 0 )
		{
			return;
		}
	}
	for( int i = 0; i < numberOfCores; i++ )
	{
		pthread_join( pthread[i], NULL );
	}

	delete pthread;

	// edge score순으로 정렬
	sort( edges.begin(), edges.end(), compEdge );

	int cntCluster = nodes.size() - cntPreClusteredNode;

	map< cid_t, vector<Node*> > clusterList;
	// 0은 아직 clustering되지 않은 노드, cntPreCluster까지는 이미 구성됨
	int newClusterID = cntPreCluster + 1;
	for( int i = 0; i < edges.size(); i++ )
	{
		int startNodeCID = edges[i].start->clusterID;
		int endNodeCID = edges[i].end->clusterID;

		// pre clustering된 edge는 건너뜀
		if( startNodeCID > 0 && startNodeCID <= cntPreCluster )
			continue;

		if( startNodeCID == 0 )
		{
			if( endNodeCID == 0 )
			{
				// 두 노드 다 아직 소속이 없는 경우, 새로운 클러스터 생성
				edges[i].start->clusterID = newClusterID;
				edges[i].end->clusterID = newClusterID;

				vector<Node*> newCluster;
				newCluster.push_back( edges[i].start );
				newCluster.push_back( edges[i].end );
				clusterList.insert( pair<cid_t, vector<Node*> >(
									newClusterID, newCluster ) );

				newClusterID++;
				cntCluster--;
			}
			else
			{
				// 시작 노드가 도착 노드의 cluster에 포함되어야 함
				if( clusterList[endNodeCID].size() + 1 > maxClusterSize)
					continue;

				edges[i].start->clusterID = endNodeCID;
				cntCluster--;

				clusterList[endNodeCID].push_back( edges[i].start );
			}
		}
		else if( endNodeCID == 0 )
		{
			// 도착 노드가 시작 노드의 cluster에 포함되어야 함
			if( clusterList[startNodeCID].size() + 1 > maxClusterSize )
				continue;

			edges[i].end->clusterID = startNodeCID;
			cntCluster--;

			clusterList[startNodeCID].push_back( edges[i].end );
		}
		else
		{
			// 두 노드 다 cluster에 소속되어 있는 경우
			// 두 노드의 실제 cluster 얻기
			if( clusterList[endNodeCID].size() + clusterList[startNodeCID].size()
				   	> maxClusterSize )
				continue;

			if( startNodeCID == endNodeCID )
			{
				// 두 노드가 이미 동일한 cluster에 소속되어 있음
				continue;
			}
			else
			{
				// 두 노드가 다른 cluster에 소속되어있음. cluster 병합
				if( clusterList[startNodeCID].size() >
					   	clusterList[endNodeCID].size() )
				{
					for( vector<Node*>::iterator it2 =
							clusterList[endNodeCID].begin();
							it2 != clusterList[endNodeCID].end(); it2++ )
					{
						(*it2)->clusterID = startNodeCID;
						clusterList[startNodeCID].push_back( *it2 );
					}
				
					//clusters[startNodeCID].insert( clusters[startNodeCID].begin(), clusters[endNodeCID].begin(), clusters[endNodeCID].end() );
					clusterList[endNodeCID].clear();
					clusterList.erase( endNodeCID );
				}
				else
				{
					for( vector<Node*>::iterator it2 =
							clusterList[startNodeCID].begin();
							it2 != clusterList[startNodeCID].end(); it2++ )
					{
						(*it2)->clusterID = endNodeCID;
						clusterList[endNodeCID].push_back( *it2 );
					}
	
					//clusters[endNodeCID].insert( clusters[endNodeCID].begin(), clusters[startNodeCID].begin(), clusters[startNodeCID].end() );
					clusterList[startNodeCID].clear();
					clusterList.erase( startNodeCID );
				}

				cntCluster--;
			}
		}

		if( cntCluster == maxClusterNum )
			break;
	}

	// clustering 되지 않은 1개짜리 node가 생긴다.
	// 이것들을 각각의 cluster로 추가
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++ )
	{
		// 크기가 1인 cluster 모두 추가
		if( it->second.clusterID == 0 )
		{
			it->second.clusterID = newClusterID;
			vector<Node*> v;
			v.push_back( &(it->second) );
			clusterList.insert( pair<cid_t, vector<Node*> >(
						newClusterID, v ) );
			newClusterID++;
		}
	}
	// cluster들을 size순으로 정렬한다
	vector< vector<Node*> > sortedCluster;
	for( map<cid_t, vector<Node*> >::iterator it = clusterList.begin();
									it != clusterList.end(); it++ )
	{
		sortedCluster.push_back( it->second );
	}
	sort( sortedCluster.begin(), sortedCluster.end(), compClusterSize );

	// 작은 cluster들끼리 뭉친다
	int currClusterSize = sortedCluster[0].size();
	int mergeClusterIdx = 0;
	for( int i = 1; i < sortedCluster.size(); i++ )
	{
		if( sortedCluster[i].size() + currClusterSize <= maxClusterSize )
		{
			currClusterSize += sortedCluster[i].size();
			for( vector<Node*>::iterator it = sortedCluster[i].begin();
								it != sortedCluster[i].end(); it++ )
			{
				(*it)->clusterID =
				   	sortedCluster[mergeClusterIdx][0]->clusterID;;
				sortedCluster[mergeClusterIdx].push_back( *it );
			}
			sortedCluster[i].clear();
		}
		else
		{
			mergeClusterIdx = i;
			currClusterSize = sortedCluster[i].size();
		}

		if( sortedCluster[i].size() >= maxClusterSize / 2 )
			break;
	}

	// cluster 번호 연속적인 숫자로 정리하기
	set<cid_t> clusterNumbers;
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++ )
	{
		clusterNumbers.insert( it->second.clusterID );
	}
	map<cid_t, cid_t> clusterNumMap;
	int num = 1;
	for( set<cid_t>::iterator it = clusterNumbers.begin();
									 it != clusterNumbers.end(); it++ )
	{
		clusterNumMap.insert( pair<cid_t, cid_t>( *it, num ));
		num++;
	}

	// Cluster 생성
	for( map<cid_t, cid_t>::iterator it = clusterNumMap.begin();
									 it != clusterNumMap.end(); it++ )
	{
		Cluster newCluster;
		newCluster.isUpdating = false;
		newCluster.size = 0;
		clusters.insert( pair<cid_t, Cluster>( it->second, newCluster ) );
	}

	// 각 node에 정리된 cluster 번호 부여, 각 cluster 크기 계산
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++ )
	{
		it->second.clusterID = clusterNumMap[it->second.clusterID];
		it->second.cluster = &(clusters[it->second.clusterID]);
		clusters[it->second.clusterID].size++;
	}

	int cntInBridge = 0;
	int cntOutBridge = 0;
	// Node의 inEdges, outEdges들을 순회하며 Bridge인지 판단, Bridge인 경우 양쪽 Cluster에 Bridge 정보를 저장하고 Node의 edge 정보에서는 제거한다. Node가 가지는 edge정보는 동일한 cluster 내의 edge만을 갖는다.
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++)
	{
		set<Node*>::iterator it2 = it->second.inEdges.begin();
		while( it2 != it->second.inEdges.end() )
		{
			if( (*it2)->clusterID != it->second.clusterID )
			{
				Bridge newBridge;
				newBridge.src = (*it2);
				newBridge.dest = &(it->second);
				ValidNode validNode;
				validNode.ver = 1;
				validNode.isValid = true;
				newBridge.validList.push_back( validNode );

//				it->second.cluster->inBridges.push_back( newBridge );
				it->second.cluster->inBridges.insert(
						pair<Node*, Bridge>( newBridge.dest, newBridge ) );

				it->second.inEdges.erase( it2++ );
				cntInBridge++;
			}
			else
				it2++;
		}
		
		it2 = it->second.outEdges.begin();
		while( it2 != it->second.outEdges.end() )
		{
			if( (*it2)->clusterID != it->second.clusterID )
			{
				Bridge newBridge;
				newBridge.src = &(it->second);
				newBridge.dest = (*it2);
				ValidNode validNode;
				validNode.ver = 1;
				validNode.isValid = true;
				newBridge.validList.push_back( validNode );

//				it->second.cluster->outBridges.push_back( newBridge );
				it->second.cluster->outBridges.insert(
					   pair<Node*, Bridge>( newBridge.src, newBridge ) );

				it->second.outEdges.erase( it2++ );
				cntOutBridge++;
			}
			else
				it2++;
		}

		it->second.inOutEdges.clear();
	}

	gettimeofday( &endTime, NULL );
	printf("<clustering time : %d>\n", endTime.tv_sec - startTime.tv_sec);

	// 리소스 정리
	clusterNumMap.clear();
	sortedCluster.clear();
	clusterList.clear();
	preCluster.clear();

	gettimeofday( &startTime, NULL );

	// multi thread로 각 edge의 score 매기기 
/*	pthread = new pthread_t[numberOfCores];
	for( long i = 0; i < numberOfCores; i++ )
	{
		if( pthread_create( &pthread[i], NULL, calcLSP, (void*)i )
			   	< 0 )
		{
			return 0;
		}
	}
	for( int i = 0; i < numberOfCores; i++ )
	{
		pthread_join( pthread[i], NULL );
	}

	delete pthread;
*/
	calcLSP(0);
/*
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
													 it != nodes.end();
													 it++ )
	{
		printf("--%d--\n", it->first);
		for( map<Node*, LSPNode*>::iterator it2 = it->second.lsp.begin();
											it2 != it->second.lsp.end();
											it2++ )
		{
			printf("%d %d\n", it2->first->nid, it2->second->dist);
		}
	}
*/
	gettimeofday( &endTime, NULL );
	printf("<preprocess time : %d>\n", endTime.tv_sec - startTime.tv_sec);

}

bool compClusterSize(vector<Node*> first, vector<Node*> second)
{
	return first.size() < second.size();
}

bool compSubgraph(vector<nid_t> first, vector<nid_t> second)
{
	return first.size() < second.size();
}

/*
	크기가 작은 subgraph들을 미리 cluster로 묶음
	return : preclustering된 각 cluster의 node수
*/
vector<int> preClustering(int maxClusterSize)
{
	queue<nid_t> q;
	vector< vector<nid_t> > subgraphs;

	// 모든 subgraph 찾기
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++ )
	{
		// subgraph로 찾은 node는 clusterID를 0으로 바꿔놓는다.
		if( it->second.clusterID == -1 )
		{
			// 새로운 subgraph 발견
			vector<nid_t> subgraph;

			it->second.clusterID = 0;
			q.push( it->first );
			subgraph.push_back( it->first );
			
			while( !q.empty() )
			{
				nid_t nodeID = q.front();
				q.pop();
				for( set<nid_t>::iterator it2 =
						nodes[nodeID].inOutEdges.begin();
					   	it2 != nodes[nodeID].inOutEdges.end(); it2++ )
				{
					if( nodes[*it2].clusterID == -1 )
					{
						nodes[*it2].clusterID = 0;
						q.push( *it2 );
						subgraph.push_back( *it2 );
					}
				}
			}
			subgraphs.push_back( subgraph );
		}
	}

	sort( subgraphs.begin(), subgraphs.end(), compSubgraph );

	// 작은 subgraph끼리 묶어서 cluster를 만든다
	int newClusterID = 1;
	int currClusterSize = 0;
	vector<int> clusterSize;
	for( int i = 0; i < subgraphs.size(); i++ )
	{
		if( subgraphs[i].size() >= maxClusterSize )
			break;

		if( subgraphs[i].size() + currClusterSize <= maxClusterSize )
		{
			for( int j = 0; j < subgraphs[i].size(); j++ )
			{
				nodes[subgraphs[i][j]].clusterID = newClusterID;
			}
			currClusterSize += subgraphs[i].size();
		}
		else
		{
			clusterSize.push_back( currClusterSize );

			newClusterID++;
			for( int j = 0; j < subgraphs[i].size(); j++ )
			{
				nodes[subgraphs[i][j]].clusterID = newClusterID;
			}
			currClusterSize = subgraphs[i].size();
		}
	}
	if( currClusterSize != 0 )
	{
		clusterSize.push_back( currClusterSize );
	}

	return clusterSize;
//	return pair<int, int>( newClusterID, cntPreClusteredNode );
}

void* calcEdgeScore(void* tid)
{
	// 각 edge의 점수 매기기
	// edge 점수 : 두 노드의 edge수 합 - 4*두 노드의 공통 이웃노드 수 - 2
	long threadID = (long)tid;

	int edgeScore;
	Node* node1;
	Node* node2;
	for( int i = threadID; i < edges.size(); i += numberOfCores )
	{
//		if( threadID == 0 )
//		{
//			if( i % (numberOfCores*1000) == 0 )
//				cout << i << endl;
//		}
		node1 = edges[i].start;
		node2 = edges[i].end;

		int cntEdge1 = node1->inOutEdges.size();
		int cntEdge2 = node2->inOutEdges.size();

		edgeScore = cntEdge1 + cntEdge2 - 2;

		// node1의 엣지가 node2의 엣지보다 작거나 같도록
		if( cntEdge1 > cntEdge2 )
		{
			Node* t = node1;
			node1 = node2;
			node2 =  t;
			SWAP( cntEdge1, cntEdge2 );
		}

		if( cntEdge1 + cntEdge2 >
			min( cntEdge1, cntEdge2 ) * log2( max( cntEdge1, cntEdge2 ) ) )
		{
			for( set<nid_t>::iterator it1 = node1->inOutEdges.begin();
									  it1 != node1->inOutEdges.end();
									  it1++ )
			{
				if( node2->inOutEdges.find( *it1 ) !=
					node2->inOutEdges.end() )
				{
					edgeScore -= 4;
				}
			}
		}
		else
		{
			set<nid_t>::iterator it1 = node1->inOutEdges.begin();
			set<nid_t>::iterator it2 = node2->inOutEdges.begin();
			while( it1 != node1->inOutEdges.end() &&
				   it2 != node2->inOutEdges.end() )
			{
				if( *it1 < *it2 )
				{
					it1++;
				}
				else if( *it1 > *it2 )
				{
					it2++;
				}
				else
				{
					edgeScore -= 4;
					it1++;
					it2++;
				}
			}
		}

		edges[i].score = edgeScore;
	}
}

bool compEdge(Edge first, Edge second)
{
	return first.score < second.score;
}

void* calcLSP(void* tid)
{
	long threadID = (long)tid;

	queue< pair<Node*, unsigned int> > q;
	set<Node*> checkSet;

	Node* node;
	pair<Node*, unsigned int> currentNode;
			
	pair<set<Node*>::iterator, bool> check;

	int i = 0;
	int cntLSPNode = 0;
	for( boost::unordered_map<nid_t, Node>::iterator it = nodes.begin();
									it != nodes.end(); it++ )
	{
//		if( ( i + numberOfCores - threadID ) % numberOfCores != 0 )
//		{
//			i++;
//			continue;
//		}

//		if( threadID == 0 )
//		{
//			if( i % 8000 == 0 )
//			{
//				printf("%d, %d\n", i, cntLSPNode);
//			}
//		}

		node = &(it->second);

		checkSet.clear();	

		q.push( pair<Node*, unsigned int>( node, 0 ) );

		while( !q.empty() )
		{
			currentNode = q.front();
			q.pop();

			for( set<Node*>::iterator it2 =
								currentNode.first->outEdges.begin();
						 it2 != currentNode.first->outEdges.end(); it2++ )
			{
				check = checkSet.insert( (*it2) );
				if( !check.second ) // 이미 탐색한 노드
				{
					continue;
				}

				// LSP에 추가
				LSPNode* newLSPNode = new LSPNode;
				newLSPNode->dist = currentNode.second + 1;
				newLSPNode->ver = 1;
				newLSPNode->next = NULL;

				pair< map<Node*, LSPNode*>::iterator, bool > ret =
								it->second.lsp.insert(
									pair<Node*, LSPNode*>(
									   	(*it2), newLSPNode ) );
				if( !ret.second )
				{
					newLSPNode->next = ret.first->second;
					ret.first->second = newLSPNode;
				}

				cntLSPNode++;

				// 이웃을 Queue에 추가
				q.push( pair<Node*, unsigned int>(
						   	*it2, currentNode.second + 1 ) );
			}
		
		}
		i++;
	}
}

