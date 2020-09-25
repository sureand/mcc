#include "mcc.h"

#define MIN_SIZE 8

static int roundup(int n) {
    if (n == 0)
        return 0;
    int r = 1;
    while (n > r)
        r  = r << 1;
    return r;
}

struct Buffer *do_make_buffer(size_t size)
{
    struct Buffer *b = (struct Buffer*)malloc(sizeof(struct Buffer));
    if(b == NULL) {
        printf("error, can not alloc !\n");
        exit(1);
    }

    size = roundup(size);
    if (size > 0) {
        b->body = malloc(sizeof(void *) * size);
    }

    b->len = 0;
    b->pos = 0;
    b->capacity = size;

    return b;
}

static void extend(struct Buffer *buf, int delta) {
    if (buf->len + delta <= buf->capacity)
        return;

    int nelem = max(roundup(buf->len + delta), MIN_SIZE);
    void *newbody = calloc(1, sizeof(void *) * nelem);

    memcpy(newbody, buf->body, sizeof(void *) * buf->len);
    free(buf->body);

    buf->body = newbody;
    buf->capacity = nelem;
}
   
struct Buffer *make_buffer()
{
    return do_make_buffer(0);
}

void buffer_push(struct Buffer *buf, void *elem)
{
   extend(buf, 1);

   buf->body[buf->len++] = elem;
}

void *buffer_pop(struct Buffer *buf)
{
    return buf->body[--buf->len];
}

void *buffer_get(struct Buffer *buf, int index)
{
    if(index < 0 || index > buf->len) {
        return NULL;
    }

    return buf->body[index];
}

void buffer_set(struct Buffer *buf, int index, void *elem)
{
    if(index < 0 || index > buf->len) {
        return;
    }
    buf->body[index] = elem;
}

void *buffer_head(struct Buffer *buf)
{
    if(buf->len == 0) {
        return NULL;
    }

    return buf->body[0];
}

void *buffer_tail(struct Buffer *buf)
{
    if(buf->len == 0) {
        return NULL;
    }

    return buf->body[buf->len];
}

void *buffer_dup(struct Buffer *buf, void *dst, size_t size)
{
    if(size > buf->capacity) {
        size = buf->capacity;
    }

    memcpy(dst, buf, size);

    return dst;
}

size_t buffer_len(struct Buffer *buf)
{
    if(buf == NULL) {
        return 0;
    }

    return buf->len;
}