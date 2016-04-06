#ifndef _QUERY_CPP_
#define _QUERY_CPP_
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include <functional>
#include "common.h"

using namespace std;
typedef multimap<Node*,Bridge>::iterator bridge_it;
typedef map<Node*,int>::iterator distmap_it;
typedef pair<Node*,int> queue_pair;
struct _MyComp{
	bool operator()(queue_pair const &a,queue_pair const &b){
		return a.second < b.second;
	}
}myComp;



priority_queue<queue_pair,vector<queue_pair>,struct _MyComp> dist_queue;
map<Node*,int> distMap;
unsigned int my_distance = UINT_MAX;
vid_t version;

void EnqueueBridges(Node* srcNode,dist_t prev_dist);
bool isBridgeSrc(Cluster* cluster, Node *node);
vid_t getLspDist(Node* src,Node* dest);
int query(Node* srcNode, Node* destNode, vid_t ver){
	Cluster* srcCluster = srcNode->cluster;
	version = ver;

	//현재 클러스터에 목적지가 있는지 확인
	int tempDist = getLspDist(srcNode,destNode);
	if(tempDist){
		my_distance = tempDist;
		dist_queue.push(queue_pair(destNode,my_distance));
	}
	EnqueueBridges(srcNode,0);
	while(1){
		pair<Node*,dist_t> cur_pair = dist_queue.top();
		dist_queue.pop();
		if(cur_pair.first == destNode){
			return cur_pair.second;
		}
		else{
			EnqueueBridges(cur_pair.first,cur_pair.second);
		}
	}
	
	return -1;
}


//지금안씀
bool isValidBridge(Bridge* bridge){
	list<ValidNode>::iterator it = bridge->validList.begin();
	bool isValid = false;
	for(;it != bridge->validList.end();++it){
		ValidNode valNode = *it;
		if(version > valNode.ver){
			isValid = valNode.isValid;
		}else{
			break;
		}
	}
	return isValid;
}


vid_t getLspDist(Node* src,Node* dest){
	map<Node*,LSPNode*>::iterator it;
	it = src->lsp.find(dest);
	if(it != src->lsp.end()){
		LSPNode* lspNode = it->second;
		dist_t dist = 0;
		for(; it!= src->lsp.end();++it){
			if(lspNode->ver >= version){
				return dist;
			}
			dist = lspNode->dist;
		}
	}
	return 0;
}
bool isBridgeSrc(Cluster* cluster, Node *node){
	pair<multimap<Node*,Bridge>::iterator,
		multimap<Node*,Bridge>::iterator> range;
	range = cluster->outBridges.equal_range(node);
	if(range.first != cluster->outBridges.end())
		return true;
	else
		return false;
}

	


void EnqueueBridges(Node* srcNode,dist_t prev_dist){

	Cluster* cluster = srcNode->cluster;

	map<Node*,LSPNode*>::iterator lspIt = srcNode->lsp.begin();


	for(; lspIt != srcNode->lsp.end();++lspIt){
		Node* destNode = lspIt->first;
		vid_t dist = getLspDist(srcNode,destNode);
		
		//거리가 전체 dest까지 거리보다 길거나 같을경우 컨티뉴
		//큐에 추가하는건 브릿지의 dest이므로 -1해서 계산
		if(prev_dist+dist >= my_distance -1){
			continue;
		}

		//LSP가 존재하는 경우
		if(!dist){
			//Bridge인지 검사
			pair<bridge_it,bridge_it> range;
			range = cluster->outBridges.equal_range(destNode);
			//Bridge가 아니라면 컨티뉴
			if(range.first == cluster->outBridges.end())
				continue;
			//Bridge일경우
			dist_t cur_dist = prev_dist + dist;
			pair<distmap_it,bool> retPair;
			retPair = distMap.insert(pair<Node*,int>(destNode,cur_dist));
			
			//Bridge 의 src가 기존 distMap에 존재할경우
			if(!retPair.second){
				//기존 distMap에 있는 값이 지금 거리보다 길 경우 BridgeDest 검사
				dist_t in_dist_bridge_src = retPair.first->second;

				//안에 있는 값이 더 크면 현재 구한 거리로 대체
				if(in_dist_bridge_src > cur_dist){
					retPair.first->second = cur_dist;
				}

			}
			//기존 distMap에 있는 값이 더 짧을 경우, 이 srcNode의 bridge 사용 안함
			else{
				continue;
			}


			for(bridge_it b_it = range.first;
					b_it != range.second;
					++b_it){
				Bridge bridge = (*b_it).second;
				pair<distmap_it,bool> temp
					= distMap.insert(pair<Node*,int>(bridge.dest,cur_dist+1));
				//Bridge의 dest값이 distMap에 있을 경우
				if(!temp.second){
					dist_t in_dist_bridge_dest = temp.first->second;
					if(in_dist_bridge_dest <= cur_dist +1){
						continue;
					}
					//기존 거리가 현재 거리보다 길 경우
					//교체하고 Bridge의 dest를 enqueue
					else{
						temp.first->second = cur_dist +1;
						dist_queue.push(queue_pair(bridge.dest,cur_dist+1));
					}
				}
			}
							
		}
	}
}




























#endif  
