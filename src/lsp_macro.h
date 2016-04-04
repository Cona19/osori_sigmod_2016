
#define LSP_UNREACHABLE 0

/*
S - Start Node
E - End Node
N - lsp iterator
PS - Passing Start Node
PE - Passing End Node
L - LSP_NEW_PATH value
*/
#define FIND(S, E) ((S).find(E))
#define IS_EXIST(N,S) ((N) != (S).end())

#define LSP_FIND(S, E) ((S)->lsp.find(E))
#define LSP_IS_EXIST(N, S) ((N) != (S)->lsp.end())

#define GET_LATEST_LSP(H) (H)
#define PUSH_LATEST_LSP(H, N) do{ \
    (N)->next = (H); \
    (H) = (N); \
}while(false)

#define IS_UNREACHABLE(D) ((D) == LSP_UNREACHABLE)
#define IS_UNREACHABLE_LSP(N) IS_UNREACHABLE(GET_LATEST_LSP((N)->second)->dist)

#define GET_LSP_DIST(S, E) (GET_LATEST_LSP((S)->lsp[E])->dist)

#define IS_NEED_TO_UPDATE(D, L) (IS_UNREACHABLE(D) || (D) > (L))
#define IS_NEED_TO_UPDATE_LSP(N, L) (IS_UNREACHABLE_LSP(N) || GET_LATEST_LSP((N)->second)->dist > (L))
/*
when you use LSP_NEW_PATH,
S -> PS and PE -> E path must be exist. otherwise, their GET_LSP_DIST can be ambiguous.(0 means either real dist is 0 or UNERACHABLE)
--deprecated--
#define LSP_NEW_PATH(S, PS, PE, E) (GET_LSP_DIST(S, PS) + 1 + GET_LSP_DIST(PE, E))
*/

