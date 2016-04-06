#include "batch_add.h"
#include "lsp_macro.h"
#include "assert.h"
#include <queue>

static bool compareAndUpdateLSP(Node* src, Node* dest, dist_t dist, vid_t ver){
    map<Node*, LSPNode*>::iterator lspIt = LSP_FIND(src, dest);
    LSPNode *new_node;

    if (LSP_IS_EXIST(lspIt, src)){
        if (!IS_NEED_TO_UPDATE_LSP(lspIt, dist)){
            //branch
            return false;
        }
        if (GET_LATEST_LSP(lspIt->second)->ver == ver)
            GET_LATEST_LSP(lspIt->second)->dist = dist;
        else{
            new_node = new LSPNode;
            new_node->dist = dist;
            new_node->ver = ver;
            PUSH_LATEST_LSP(lspIt->second, new_node);
        }
    }
    else{
        new_node = new LSPNode;
        new_node->dist = dist;
        new_node->ver = ver;
        PUSH_LATEST_LSP(src->lsp[dest], new_node);
    }
    return true;
}

static bool updateLSP(Node* src, Node* dest, Node* updatedNode, vid_t curr_ver){
    //In this case, there is no update because it always makes cycle.
    if (dest == updatedNode){
        return false;
    }
#ifdef ASSERT_MODE
    ASSERT(src == updatedNode || LSP_IS_EXIST(LSP_FIND(updatedNode, src), updatedNode));
#endif

    queue<Node*> nodeQueue;
    set<Node*> foundCheck;
    Node* currNode;
    dist_t distAPart = (src == updatedNode ? 0 : GET_LSP_DIST(updatedNode, src));

    {
        map<Node* , LSPNode*>::iterator lspIt = LSP_FIND(updatedNode, dest);
        //if this node don't have to use this new edge, return false;
        if (LSP_IS_EXIST(lspIt, updatedNode) && !IS_NEED_TO_UPDATE_LSP(lspIt, distAPart + 1)){
            return false;//update is not used
        }
    }

    nodeQueue.push(dest);
    foundCheck.insert(dest);
    
    foundCheck.insert(updatedNode);//updatedNode itself don't need to explore => cycle is always not used as shortest

    while(!nodeQueue.empty()){
        currNode = nodeQueue.front();
        nodeQueue.pop();

#ifdef ASSERT_MODE
        ASSERT(dest == currNode || LSP_IS_EXIST(LSP_FIND(dest, currNode), dest));
#endif
        //if this node don't have to use this new edge, branch it;
        if (compareAndUpdateLSP(updatedNode, currNode, distAPart + 1 + (dest == currNode ? 0 : GET_LSP_DIST(dest, currNode)), curr_ver)){
            for (set<Node*>::iterator it = currNode->outEdges.begin(); it != currNode->outEdges.end(); it++){
                if (foundCheck.find(*it) == foundCheck.end()){
                    nodeQueue.push(*it);
                    foundCheck.insert(*it);
                }
            }
        }
    }
    return true;
}

/*
1. src에 도달가능한 node들을 src부터 역으로 bfs탐색한다.
2. 발견한 node n1에 대해서 dest시작으로 bfs탐색을 한번 더 수행한다.
    1) 여기서 발견된 노드 n2에 대해서 경로가 갱신가능한지를 확인한다.
        (1) 갱신가능하다면 n2에 연결된 다음 노드들을 탐색하여 큐에 추가한다.
        (2) 갱신이 불가능하다면 더이상 찾지 않고 n2에 대한 탐색을 종료한다.
    2) 만약 dest노드부터 갱신이 불가능하다면 n1의 이전 노드들을 탐색하지 않고 종료한다.

*/
void addEdge(Node* src, Node* dest, vid_t curr_ver){
    queue<Node*> nodeQueue;
    set<Node*> foundCheck;
    Node* currNode;

#ifdef ASSERT_MODE
    ASSERT(src != dest);
    ASSERT(src->cluster == dest->cluster);
    ASSERT(src->cluster->isUpdating);
#endif
    if (IS_EXIST(FIND(src->outEdges, dest), src->outEdges)){
#ifdef ASSERT_MODE
        ASSERT(IS_EXIST(FIND(dest->inEdges, src), dest->inEdges));
#endif
        return;
    }
    src->outEdges.insert(dest);
    dest->inEdges.insert(src);


    nodeQueue.push(src);
    foundCheck.insert(src);
    while(!nodeQueue.empty()){
        currNode = nodeQueue.front();
        nodeQueue.pop();

        if (!updateLSP(src, dest, currNode, curr_ver)) continue;

        for (set<Node*>::iterator it = currNode->inEdges.begin(); it != currNode->inEdges.end(); it++){
            if (!IS_EXIST(FIND(foundCheck,*it), foundCheck)){
                nodeQueue.push(*it);
                foundCheck.insert(*it);
            }
        }
    }
    src->cluster->isUpdating = false;
}
