#ifndef _COMMON_H_
#define _COMMON_H_

#include <set>
#include <list>
#include <map>
#include <boost/unordered_map.hpp>

using namespace std;

typedef unsigned int nid_t;
typedef unsigned int cid_t;
typedef unsigned int vid_t;
typedef unsigned int dist_t;

typedef struct _ValidNode
{
	vid_t ver;
	bool isValid;
} ValidNode;

typedef struct _LSPNode
{
	dist_t dist;
	vid_t ver;
	_LSPNode* next;
} LSPNode;

typedef struct _Bridge
{
	struct _Node* src;
	struct _Node* dest;
	list<ValidNode> validList;
} Bridge;

typedef struct _Cluster
{
//	list<Bridge> inBridges;
//	list<Bridge> outBridges;
	multimap<struct _Node*, Bridge> inBridges;	// key : dest node
	multimap<struct _Node*, Bridge> outBridges;	// key : src node
	bool isUpdating;
	unsigned int size;
} Cluster;

typedef struct _Node
{
	Cluster* cluster;
	set<_Node*> inEdges;
	set<_Node*> outEdges;
	map<_Node*, LSPNode*> lsp;

	// only for clustering
	set<nid_t> inOutEdges;
	cid_t clusterID;
	nid_t nid;
} Node;

#endif
