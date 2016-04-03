
#define IS_EXIST(X,Y) ((X).find(Y) != (X).end())

/*
S - Start Node
E - End Node
N - lsp iterator
PS - Passing Start Node
PE - Passing End Node
L - LSP_NEW_PATH value
*/
#define LSP_FIND(S, E) ((S)->lsp.find(E))
#define LSP_IS_EXIST(N, S) ((N) != (S)->lsp.end())

#define GET_LATEST_LSP(H) H
#define PUSH_LATEST_LSP(H, N) do{ \
    N->next = H; \
    H = N; \
}while(false)

#define GET_LSP_DIST(S, E) ((S) == (E) ? 0 : GET_LATEST_LSP((S)->lsp[E])->dist)

#define LSP_NEW_PATH(S, PS, PE, E) (GET_LSP_DIST(S, PS) + 1 + GET_LSP_DIST(PE, E))
                                     
#define LSP_IS_NEED_TO_UPDATE(N, L) (GET_LATEST_LSP((N)->second)->dist > (L))
