#include "mcc.h"

struct Dict *make_dict() {
    struct Dict *r = malloc(sizeof(struct Dict));
    r->map = make_map();
    r->key = make_buffer();
    return r;
}

void *dict_get(struct Dict *dict, char *key) {
    return map_get(dict->map, key);
}

void dict_put(struct Dict *dict, char *key, void *val) {
    map_put(dict->map, key, val);
    buffer_push(dict->key, key);
}

struct Buffer *dict_keys(struct Dict *dict) {
    return dict->key;
}
