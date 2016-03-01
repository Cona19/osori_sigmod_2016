/*
   mvgraph.h
   written by Jongbin Kim
   2016. 02. 23.

   Data structure to manipulate multiversion graph edges
*/

#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <utility>
#include <config.h>

using namespace std;

#define HASH_MAX	1000003
#define HASH_NODE(i) ( i % HASH_MAX )

#define QUEUE_MAX	1000000

typedef struct _Edge
{
	unsigned int	destNodeNum_;	// destination node number
	struct _Node*	destNode_;		// destination node of this edge
	int				version_;		// transaction number which made this
	_Edge*			next_;			// next edge with same starting node
} Edge;

typedef struct _Node
{
	unsigned int	nodeNum_;		// node number
	_Edge*			edge_;			// directed edge to other node(list)
	_Node*			next_;			// next node in same hash bucket
//	unsigned int	refCount_;		// number of nodes indicating this
	_Edge*			revEdge_;		// reverse edge indicating this
	int				bfsFlag_[NUM_THREAD];
} Node;

class MVGraph
{
public:
	MVGraph();
	~MVGraph();

	void addEdge(const unsigned int s,
				 const unsigned int e,
				 const int version);
	void removeEdge(const unsigned int s,
					const unsigned int e,
					const int version);

	int	 findSPP(const unsigned int s,
				 const unsigned int e,
				 const int version,
				 const long tid,
				 pair<Node*, int>* q);

	void garbageCollect();

private:
	Node*	nodes_[HASH_MAX];

public:
	int		cntEdge_;				// number of edges in the graph
	int		cntRemovedEdge_;		// number of edges marked as removed
};

#endif
