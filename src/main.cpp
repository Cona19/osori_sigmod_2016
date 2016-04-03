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

	cout << "R" << endl;

}

