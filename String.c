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

struct String *do_make_String(size_t size)
{
    struct String *str = (struct String *)calloc(1, sizeof(struct String));

    size = roundup(size);
    if (size > 0) {
        str->data = calloc(1, sizeof(void *) * size);
    }

    str->len = size;
    
    return str;
}

struct String *make_String()
{
    return do_make_String(0);
}

static void extend(struct String *str, int delta)
{
    if(str->len + delta < str->capacity) {
        return;
    }

    int new_len = max(roundup(str->len + delta), MIN_SIZE);
    char *newstr = (char*)calloc(1, sizeof(char) * new_len + 1);
    str->capacity = new_len;

    strncpy(newstr, str->data, str->len);
    free(str->data);

    str->data = newstr;
}

static void extend_string(struct String *str, const char *s)
{
    extend(str, strlen(s));

    strcat(str->data, s);
}

char *String_add(struct String *str, char *s)
{
    extend_string(str, s);

    return str->data;
}

char *String_append(struct String *str, char c)
{
    extend(str, 1);

    str->data[str->len++] = c;
    str->data[str->len] = '\0';

    return str->data;
}

char *String_dup(struct String *str)
{
    return strdup(str->data);
}

size_t String_len(struct String *str)
{
    return str->len;
}

char *String_move(struct String *str)
{
    char *s = strdup(str->data);
    free(str->data);

    str->data = NULL;
    str->len = 0;

    return s;
}