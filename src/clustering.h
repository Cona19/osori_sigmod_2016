#include <vector>
#include "common.h"

typedef struct _Edge
{
	Node* start;
	Node* end;
	int score;
} Edge;

void clustering();

