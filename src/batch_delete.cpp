#include "batch_delete.h"
#include "lsp_macro.h"
#include "my_assert.h"
#include <queue>

typedef struct _bfsNode{
    Node* node;
    dist_t dist;
} bfsNode;

/*
1. A그룹인 애들을 BFS로 찾고, B그룹인 애들을 lsp로 찾는다.
2. A그룹의 애를 하나씩 for문돌린다. (a)
    1) a를 시작으로 BFS를 돌린다.
    2) 도착한 애가 C그룹이면 더이상 탐색하지 않고 그 lsp값을 사용한다.
    3) 도착한 애가 A그룹에 속하면 B에 속하는 애들에 대한 lsp값을 믿을 수 없다.(나머지는 믿을 수 있다.
    4) 도착한 애가 A그룹에 속하지 않고 B그룹에 속하면 더이상 탐색하지 않고 lsp값을 사용한다.
*/

set<Node*> getAGroupSet(Node* src){
    queue<Node*> nodeQueue;
    set<Node*> foundCheck;
    Node* currNode;

    nodeQueue.push(src);
    foundCheck.insert(src);

    while(!nodeQueue.empty()){
        currNode = nodeQueue.front();
        nodeQueue.pop();

        for (set<Node*>::iterator it = currNode->inEdges.begin(); it != currNode->inEdges.end(); it++){
            if (!IS_EXIST(FIND(foundCheck, *it), foundCheck)){
                nodeQueue.push(*it);
                foundCheck.insert(*it);
            }
        }
    }
    return foundCheck;
}

/*
Not all B Group memebers, just those which is not believable.
*/
map<Node*, dist_t> getBGroupSet(Node* bStart, Node* aNode, dist_t toDelEdge){
#ifdef ASSERT_MODE
    ASSERT(bStart != aNode);
#endif
    queue<Node*> nodeQueue;
    map<Node*, dist_t> foundCheck;
    Node* currNode;

    //prev toDelEdge refers to distance from aNode to source node which has the deleted edge.
    toDelEdge = toDelEdge + 1;
    //after add 1 to toDelEdge, this refers to distance from aNode to destination node which has the deleted edge.

    //This means if toDelEdge does not equal to lsp from aNode to currNode, this node doesn't use the deleted edge at all.
    if (toDelEdge == GET_LSP_DIST(aNode, bStart)){
        nodeQueue.push(bStart);
        foundCheck[bStart] = LSP_UNREACHABLE;

        while(!nodeQueue.empty()){
            currNode = nodeQueue.front();
            nodeQueue.pop();

            for (set<Node*>::iterator it = currNode->outEdges.begin(); it != currNode->outEdges.end(); it++){
                //push if it is unbelievable
#ifdef ASSERT_MODE
                ASSERT(bStart == *it || (IS_EXIST(FIND(bStart->lsp, *it), bStart->lsp) && !IS_UNREACHABLE(GET_LSP_DIST(bStart, *it))));
                ASSERT(aNode == *it || (IS_EXIST(FIND(aNode->lsp, *it), aNode->lsp) && !IS_UNREACHABLE(GET_LSP_DIST(aNode, *it))));
#endif
                if (!IS_EXIST(FIND(foundCheck, *it),foundCheck) && 
                    toDelEdge + (bStart == *it ? 0 : GET_LSP_DIST(bStart, *it)) == (aNode == *it ? 0 : GET_LSP_DIST(aNode, *it))){
                    nodeQueue.push(*it);
                    foundCheck[*it] = LSP_UNREACHABLE;
                }
            }
        }
    }
    return foundCheck;
}

static inline void verCheckAndUpdateLSP(Node *src, Node *dest, dist_t dist, vid_t ver){
#ifdef ASSERT_MODE
    ASSERT(src != dest);
    ASSERT(IS_EXIST(FIND(src->lsp, dest),src->lsp) && !IS_UNREACHABLE(GET_LSP_DIST(src, dest)));
#endif
    map<Node*, LSPNode*>::iterator lspIt = LSP_FIND(src, dest);

    if (GET_LATEST_LSP(lspIt->second)->ver == ver){
        GET_LATEST_LSP(lspIt->second)->dist = dist;
    }
    else{
        LSPNode *newNode = new LSPNode;
        newNode->dist = dist;
        newNode->ver = ver;
        PUSH_LATEST_LSP(lspIt->second, newNode);
    }
}

static void updateLSP(Node* src, set<Node*> &aGroup, map<Node*, dist_t> &bGroup, vid_t currVer){
#ifdef ASSERT_MODE
    ASSERT(!bGroup.empty());
#endif
    queue<bfsNode> nodeQueue;
    set<Node*> foundCheck;
    bfsNode currBfsNode = {src, 0};
    Node* currNode;
    map<Node*, LSPNode*>::iterator lspIt;
    map<Node*, dist_t>::iterator bgrIt;

    nodeQueue.push(currBfsNode);
    foundCheck.insert(src);

/*
A그룹이면 탐색
아니면 값 이용하고 탐색x
*/
    while(!nodeQueue.empty()){
        currBfsNode = nodeQueue.front();
        nodeQueue.pop();

        currNode = currBfsNode.node;

        if (src != currNode){
            bgrIt = FIND(bGroup,currNode);
            if (IS_EXIST(bgrIt, bGroup)){
                verCheckAndUpdateLSP(src, currNode, currBfsNode.dist, currVer);
                bGroup.erase(bgrIt);
                if (bGroup.empty()){
                    return;
                }
            }
        }

        if (IS_EXIST(FIND(aGroup, currNode), aGroup)){
            //set dist as dist +1 to use in queue push.
            currBfsNode.dist++;
            for (set<Node*>::iterator it = currNode->outEdges.begin(); it != currNode->outEdges.end(); it++){
                if (!IS_EXIST(FIND(foundCheck,*it),foundCheck)){
                    bgrIt = FIND(bGroup,*it);
                    if (IS_EXIST(bgrIt, bGroup)){
                        if (IS_NEED_TO_UPDATE(bgrIt->second, currBfsNode.dist)){
                            currBfsNode.node = *it;
                            nodeQueue.push(currBfsNode);
                            foundCheck.insert(*it);
                        }
                        else{
                            bGroup.erase(bgrIt);
                            if (bGroup.empty()){
                                return;
                            }
                        }
                    }
                    else{
                        currBfsNode.node = *it;
                        nodeQueue.push(currBfsNode);
                        foundCheck.insert(*it);
                    }
                }
            }
        }
        else{
            for (map<Node*, dist_t>::iterator it = bGroup.begin(); it != bGroup.end(); it++){
                lspIt = LSP_FIND(currNode, it->first);
                if (LSP_IS_EXIST(lspIt, currNode) && !IS_UNREACHABLE_LSP(lspIt)){
                    dist_t newDist = currBfsNode.dist + GET_LATEST_LSP(lspIt->second)->dist;
                    if (IS_NEED_TO_UPDATE(it->second, newDist)){
                        it->second = newDist;
                    }
                }
            }
        }
    }
    for (map<Node*, dist_t>::iterator it = bGroup.begin(); it != bGroup.end(); it++){
        verCheckAndUpdateLSP(src, it->first, it->second, currVer);
    }
}

void deleteEdge(Node* src, Node* dest, vid_t currVer){
    set<Node*> aGroup;
    map<Node*, dist_t> bGroup;
    set<Node*>::iterator tmp;

#ifdef ASSERT_MODE
    ASSERT(src != dest);
    ASSERT(src->cluster == dest->cluster);
    ASSERT(src->cluster->isUpdating);
#endif

    if (!IS_EXIST(FIND(src->outEdges, dest), src->outEdges)){
#ifdef ASSERT_MODE
        ASSERT(!IS_EXIST(FIND(dest->inEdges, src), dest->inEdges));
#endif
        return;
    }
    src->outEdges.erase(dest);
    dest->inEdges.erase(src);

    aGroup = getAGroupSet(src);
/*
A그룹이면 탐색
아니면 값 이용하고 탐색x
*/
    for (set<Node*>::iterator it = aGroup.begin(); it != aGroup.end();){
        if (dest != *it){
            bGroup = getBGroupSet(dest, *it, (*it == src) ? 0 : GET_LATEST_LSP((*it)->lsp[src])->dist);
            if (!bGroup.empty()){
                updateLSP(*it, aGroup, bGroup, currVer);
            }
        }
        tmp = it;
        it++;
        aGroup.erase(*tmp);
    }
    src->cluster->isUpdating = false;
}
