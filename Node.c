#include "mcc.h"
struct Node *make_node()
{
    struct Node *node = (struct Node *)calloc(1, sizeof(struct Node));

    return node;
}

struct TYPE *make_type()
{
    struct TYPE *ty = (struct TYPE *)calloc(1, sizeof(struct TYPE));

    ty->bitsize = -1;

    return ty;
}