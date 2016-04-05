#include "batch_add.h"
#include "lsp_macro.h"
#include <queue>
#include <stdio.h>

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
    if (dest == updatedNode){
        return false;
    }
    queue<Node*> nodeQueue;
    set<Node*> foundCheck;
    Node* curr_node;
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
        curr_node = nodeQueue.front();
        nodeQueue.pop();

        //if this node don't have to use this new edge, branch it;
        if (compareAndUpdateLSP(updatedNode, curr_node, distAPart + 1 + (dest == curr_node ? 0 : GET_LSP_DIST(dest, curr_node)), curr_ver)){
            for (set<Node*>::iterator it = curr_node->outEdges.begin(); it != curr_node->outEdges.end(); it++){
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
    Node* curr_node;

	if (IS_EXIST(FIND(src->outEdges, dest), src->outEdges)){
        return;
    }
    src->outEdges.insert(dest);
    dest->inEdges.insert(src);

    nodeQueue.push(src);
    foundCheck.insert(src);

    while(!nodeQueue.empty()){
        curr_node = nodeQueue.front();
        nodeQueue.pop();

        if (!updateLSP(src, dest, curr_node, curr_ver)) continue;

        for (set<Node*>::iterator it = curr_node->inEdges.begin(); it != curr_node->inEdges.end(); it++){
            if (foundCheck.find(*it) == foundCheck.end()){
                nodeQueue.push(*it);
                foundCheck.insert(*it);
            }
        }
    }
}
