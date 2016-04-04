#pragma once
#ifdef ASSERT_MODE
#include <stdio.h>
#include <stdlib.h>

#define ASSERT(C) do{ \
    if (C){ \
        printf(#C" Error"); \
        exit(0);
    } \
while(false)
#endif
