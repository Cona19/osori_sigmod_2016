/*
   mvgraph.cpp
   written by Jongbin Kim
   2016. 02. 23.

   Implementation of MVGraph
*/

#include <stdio.h>
#include <string.h>
#include <iostream>
#include "mvgraph.h"

MVGraph::MVGraph()
{
	for( int i = 0; i < HASH_MAX; i++ )
		nodes_[i] = NULL;

	cntEdge_ = 0;
	cntRemovedEdge_ = 0;
}

MVGraph::~MVGraph()
{

}

/*
   add edge information into both start and end node,
   to run BFS from both side
*/
void MVGraph::addEdge(const unsigned int s,
					  const unsigned int e,
					  const int version)
{
	int sidx = HASH_NODE(s);
	int eidx = HASH_NODE(e);

	Node* startNode = nodes_[sidx];
	Node* endNode = nodes_[eidx];
	Node* pNodeS = NULL;
	Node* pNodeE = NULL;
	Node* snode;
	Node* enode;
	bool isNewNodeS = false, isNewNodeE = false;

	// find end node
	while( endNode != NULL && endNode->nodeNum_ != e )
	{
		pNodeE = endNode;
		endNode = endNode->next_;
	}
	if( endNode == NULL )
	{
		// there is no end node in graph. make new one.
		enode = new Node;
		enode->nodeNum_ = e;
		enode->next_ = NULL;
		enode->edge_ = NULL;
		enode->revEdge_ = NULL;
		memset( enode->bfsFlag_, 0, NUM_THREAD * sizeof(unsigned int) );

		endNode = enode;
		isNewNodeE = true;
	}

	// find start node
	while( startNode != NULL && startNode->nodeNum_ != s )
	{
		pNodeS = startNode;
		startNode = startNode->next_;
	}

	if( startNode == NULL )
	{
		// there is no start node in the graph. make new one.
		snode = new Node;
		snode->nodeNum_ = s;
		snode->next_ = NULL;
		snode->edge_ = NULL;
		snode->revEdge_ = NULL;
		memset( snode->bfsFlag_, 0, NUM_THREAD * sizeof(unsigned int) );

		startNode = snode;
		isNewNodeS = true;
	}

	// make directed edge
	Edge* edge = new Edge;
	edge->destNodeNum_ = e;
	edge->destNode_ = endNode;				// link to dest node
	edge->version_ = version;
	edge->next_ = startNode->edge_;

	// make reverse edge
	Edge* edgeRev = new Edge;
	edgeRev->destNodeNum_ = s;
	edgeRev->destNode_ = startNode;			// rev link to start node
	edgeRev->version_ = version;
	edgeRev->next_ = endNode->revEdge_;
	
	__sync_synchronize();

	startNode->edge_ = edge;
	endNode->revEdge_ = edgeRev;

	// add node in graph if this node is a newbie
	if( isNewNodeE )
	{
		if( pNodeE == NULL )
			nodes_[eidx] = enode;
		else
			pNodeE->next_ = enode;
	}
	if( isNewNodeS )
	{
		if( pNodeS == NULL )
		{
			if( sidx == eidx )				// two new node in same bucket
				nodes_[eidx]->next_ = snode;
			else
				nodes_[sidx] = snode;
		}
		else
			pNodeS->next_ = snode;
	}

	cntEdge_++;
}

void MVGraph::removeEdge(const unsigned int s,
						 const unsigned int e,
						 const int version)
{
	int sidx = HASH_NODE(s);
	int eidx = HASH_NODE(e);

	Node* startNode = nodes_[sidx];
	while( startNode != NULL && startNode->nodeNum_ != s )
	{
		startNode = startNode->next_;
	}
	if( startNode == NULL )
	{
		// there is no start node
		return;
	}

	Node* endNode = nodes_[eidx];
	while( endNode != NULL && endNode->nodeNum_ != e )
	{
		endNode = endNode->next_;
	}
	if( endNode == NULL )
	{
		// there is no end node
		return;
	}
	
	Edge* edge = startNode->edge_;
	while( edge != NULL && edge->destNodeNum_ != e )
	{
		edge = edge->next_;
	}
	if( edge == NULL )
	{
		// there is no edge start from s
		return;
	}

	Edge* edgeRev = endNode->revEdge_;
	while( edgeRev != NULL && edgeRev->destNodeNum_ != s )
	{
		edgeRev = edgeRev->next_;
	}
	if( edgeRev == NULL )
	{
		// there is no reverse edge start from e
		return;
	}

	// remove edge, by changing version to -version
	edge->version_ = -version;
	edgeRev->version_ = -version;

	cntRemovedEdge_++;
}

int MVGraph::findSPP(const unsigned int s,
					 const unsigned int e,
					 const int version,
					 const long tid,
					 pair<Node*, int>* q)
{
	if( s == e ) return 0;

	int front = 0, rear = 0;

	Node* startNode;
	Node* endNode;

	int sidx = HASH_NODE(s);
	int eidx = HASH_NODE(e);

	startNode = nodes_[sidx];
	while( startNode != NULL && startNode->nodeNum_ != s )
	{
		startNode = startNode->next_;
	}
	if( startNode == NULL )
	{
		// there is no start node in graph
		return -1;
	}

	endNode = nodes_[eidx];
	while( endNode != NULL && endNode->nodeNum_ != e )
	{
		endNode = endNode->next_;
	}
	if( endNode == NULL )
	{
		// there is no end node in graph, or no link to end node
		return -1;
	}
	
	// check starting point as already visited node
	startNode->bfsFlag_[tid] = version;
	endNode->bfsFlag_[tid] = -version;

	// make queue, enque starting point & end point
	// search from not only starting point but also end point
	pair<Node*, int> seed = make_pair(startNode, 1);
	pair<Node*, int> seed2 = make_pair(endNode, -1);
	q[rear] = seed;
	q[rear+1] = seed2;
	rear = (rear+2) % QUEUE_MAX;

	Edge* edge;
	pair<Node*, int> p;
	pair<Node*, int> neighbor;

	bool isRevSearch = true;
	bool isAdded = true;

	int testCnt = 0;
	while( front != rear )
	{
		// deque
		p = q[front];
		front = (front+1) % QUEUE_MAX;

		if( p.second > 0 )
		{
			// searching normal direction
			if( isRevSearch )
			{
				// switch searching direction to normal
				isRevSearch = false;
				if( !isAdded )
				{
					// no neighbor added in past reverse searching.
					// it means that there is no path.
					return -1;
				}
				isAdded = false;
			}

			edge = p.first->edge_;
			while( edge != NULL )
			{
				//  if edge->version is less then 0 and its absolute value
				// is higher then version, then it will be removed later.
				// skip high version edge, which made in future
				if( ( edge->version_ >= 0 && edge->version_ < version ) ||
					( edge->version_ < 0 && edge->version_*-1 >= version ) )
				{
					if( edge->destNodeNum_ == e )
					{
						// two node linked directly.
						return 1;
					}
					// check if this node already visit
					if( edge->destNode_->bfsFlag_[tid] == version )
					{
						// already visited
						edge = edge->next_;
						continue;
					}
					if( edge->destNode_->bfsFlag_[tid] == -version )
					{
						// reverse searching already visited this node.
						// path found.
						return p.second * 2 - 1;
					}
					edge->destNode_->bfsFlag_[tid] = version;

					// enque neighbor node with +1 depth
					neighbor = make_pair( edge->destNode_, p.second + 1 );
					q[rear] = neighbor;
					rear = (rear+1) % QUEUE_MAX;
					isAdded = true;
					testCnt++;
				}
				edge = edge->next_;
			}
		}
		else
		{
			// search reverse direction
			if( !isRevSearch )
			{
				// switch searching direction to reverse
				isRevSearch = true;
				if( !isAdded )
				{
					// no neighbor added in past normal searching.
					// it means that there is no path.
					return -1;
				}
				isAdded = false;
			}

			edge = p.first->revEdge_;
			while( edge != NULL )
			{
				//  if edge->version is less then 0 and its absolute value
				// is higher then version, then it will be removed later.
				// skip high version edge, which made in future
				if( ( edge->version_ >= 0 && edge->version_ < version ) ||
					( edge->version_ < 0 && edge->version_*-1 > version ) )
				{
					// check if this node already visit
					if( edge->destNode_->bfsFlag_[tid] == -version )
					{
						// already visited
						edge = edge->next_;
						continue;
					}
					if( edge->destNode_->bfsFlag_[tid] == version )
					{
						// normal searching already visited this node.
						// path found.
						return p.second * -2;
					}
					edge->destNode_->bfsFlag_[tid] = -version;

					// enque neighbor node with +1 depth
					neighbor = make_pair( edge->destNode_, p.second - 1 );
					q[rear] = neighbor;
					rear = (rear+1) % QUEUE_MAX;
					isAdded = true;
					testCnt++;
				}
				edge = edge->next_;
			}	
		}
	}
	// there's no path
	return -1;
}

void MVGraph::garbageCollect()
{
	int i;
	Node* node;
	Node* pNode;
	Edge* edge;
	Edge* pEdge;

	for( i = 0; i < HASH_MAX; i++ )
	{
		node = nodes_[i];
		pNode = NULL;

		while( node != NULL )
		{
			// remove marked edges in node
			pEdge = NULL;
			edge = node->edge_;
			while( edge != NULL )
			{
				if( edge->version_ < 0 )
				{
					// marked edge as removed
					if( pEdge == NULL )
					{
						node->edge_ = edge->next_;
						delete edge;
						edge = node->edge_;
					}
					else
					{
						pEdge->next_ = edge->next_;
						delete edge;
						edge = pEdge->next_;
					}
					cntEdge_--;
					cntRemovedEdge_--;
					continue;
				}

				pEdge = edge;
				edge = edge->next_;
			}

			// removed marked reverse edges in node
			pEdge = NULL;
			edge = node->revEdge_;
			while( edge != NULL )
			{
				if( edge->version_ < 0 )
				{
					// marked edge as removed
					if( pEdge == NULL )
					{
						node->revEdge_ = edge->next_;
						delete edge;
						edge = node->revEdge_;
					}
					else
					{
						pEdge->next_ = edge->next_;
						delete edge;
						edge = pEdge->next_;
					}
					continue;
				}

				pEdge = edge;
				edge = edge->next_;
			}

			if( node->edge_ == NULL && node->revEdge_ == NULL )
			{
				// this node is seperated in graph. can be removed.
				if( pNode == NULL )
				{
					nodes_[i] = node->next_;
					delete node;
					node = nodes_[i];
				}
				else
				{
					pNode->next_ = node->next_;
					delete node;
					node = pNode->next_;
				}
				continue;
			}

			pNode = node;
			node = node->next_;
		}
	}
}

