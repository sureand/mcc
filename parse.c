#include "mcc.h"

struct Map *global_env = NULL;
struct Map *local_env = NULL;

struct Map *tags = NULL;

struct Buffer *cases = NULL;

static char *long_continue = NULL;
static char *long_break = NULL;
static char *default_case = NULL;

struct Buffer *local_vars = NULL;
struct TYPE *func_type = NULL;
struct Map *local_labels = NULL;

int max(int a, int b)
{
    return a > b ? a : b;
}

struct Map *env()
{
    return global_env ? global_env : local_env;
}

struct Map *tag_env()
{
    return tags;
}

struct Map *set_env(struct Map *e)
{
    local_env = e;
    return local_env;
}

void *is_in_tags(const char *field)
{
    return map_get(tags, (char*)field);
}

void put_tag(const char *field, void *data)
{
    map_put(tags, field, data);
}

char *make_temp_name()
{
    static size_t c = 0;
    char *name = calloc(20, 1);
    sprintf(name, ".T%ld", c++);

    return name;
}

char *make_label()
{
    static size_t pos = 0;
    char *label = calloc(20, sizeof(char));

    sprintf(label, ".L%ld", pos++);

    return label;
}

static struct TYPE *make_array_type(struct TYPE *ty, int len)
{
    struct TYPE *ty_array = make_type();
    ty_array->len = len;
    ty_array->align = ty->align;
    ty_array->size = (len == -1) ? -1 : ty->size * len;

    return ty_array;
}

static struct TYPE *make_func_type(struct TYPE *ret, struct Buffer *params)
{
    struct TYPE *ty_func = make_type();

    ty_func->ret = ret;
    ty_func->params = params;

    return ty_func;
}

static struct TYPE *make_ptr_type(struct TYPE *ty)
{
    struct TYPE *ty_ptr = make_type();
    ty_ptr->kind = KIND_POINTER;

    ty_ptr->size = 8;
    ty->ptr->align = 8;

    ty_ptr->ptr = ty;

    return ty_ptr;
}

//base type include int, float, double, string, long
static struct TYPE *make_base_type(int kind, int size, int algin, BOOL is_unsigned)
{
    struct TYPE *ty = make_type();
    ty->kind = kind;
    ty->size = size;
    ty->align = algin;
    ty->is_unsigned = is_unsigned;

    return ty;
}

static struct TYPE *make_int_type()
{
    return make_base_type(KIND_INTEGER, 4, 4, FALSE);
}

static struct TYPE *make_char_type()
{
    return make_base_type(KIND_CHAR, 1, 1, FALSE);
}

static struct TYPE *make_double_type()
{
    return make_base_type(KIND_CHAR, 8, 8, FALSE);
}

struct TYPE *make_string_type()
{
    struct TYPE *ty = make_type();
    ty->kind = KIND_STRING;

    return ty;
}

struct TYPE *make_enum_type()
{
    struct TYPE *ty = make_type();
    ty->kind = KIND_INTEGER;
    ty->size = 4;
    ty->align = 4;

    return ty;
}

static struct TYPE *make_temp_type() 
{
    struct TYPE *ty = make_type();
    ty->kind = KIND_TEMP;

    return ty;
}

struct TYPE *make_struct_type()
{
    struct TYPE *ty = make_type();
    ty->kind = KIND_STRUCT;
    ty->size = -1;

    ty->fields = make_dict();

    return ty;
}

struct TYPE *make_union_type()
{
    struct TYPE *ty = make_type();
    ty->kind = KIND_UNION;
    ty->size = -1;

    ty->fields = make_dict();

    return ty;
}

struct Node *make_unary_node(int id, struct TYPE *ty, struct Node *operand)
{
    struct Node *node = make_node();
    node->kind = AST_UNARY;
    node->ty = ty;
    node->id = id;
    node->operand = operand;

    return node;
}

struct Node *make_deref_node(struct TYPE *ty, struct Node *operand)
{
    return make_unary_node('&', ty, operand);
}

static struct Node *make_number_node(struct TYPE *ty)
{
    struct Node *node = make_node();
    node->kind = KIND_NUMBER;
    node->ty = ty;

    return node;
}

struct Node *make_int_node(struct TYPE *ty, long var)
{
    struct Node *node = make_node();
    node->kind = AST_LITERAL;
    node->ty = ty;
    node->ival = var;

    return node;
}

struct Node *make_string_node(char *str)
{
    struct Node *node = make_node();

    node->ty = make_string_type();

    node->kind = AST_LITERAL;
    node->strval = str;

    return node;
}

struct Node *make_float_node(struct TYPE *ty, double var)
{
    struct Node *node = make_node();
    node->kind = AST_LITERAL;
    node->ty = ty;
    node->fval = var;

    return node;
}

struct Node *make_jump_node(const char *label)
{
    struct Node *node = make_node();
    node->kind = AST_GOTO;
    node->label = label;

    return node;
}

struct Node *make_label_node(const char *lab)
{
    struct Node *node = make_node();
    node->kind = AST_LABEL;
    node->label = lab;

    //不能出现相同的label
    if(map_get(local_labels, lab)) {
        error_force("duplicate label: %s", lab);
    }

    map_put(local_labels, lab, node);

    return node;
}

struct Node *read_octal(const char *number)
{
    long sum = 0;
    
    char *p = number;
    while(*p) {
        sum = sum * 8 + *p - '0';
    }

    return make_int_node(make_int_type(), sum);
}

struct Node *read_decimal(const char *number)
{
    double sum = 0;
    
    char *p = number;
    while(*p) {
        sum = sum * 10 + *p - '0';
    }

    if(*p && *p == '.') {
        ++p;
        int sign = -1;
        int len = 0;
        while(*p) {
            sum = sum * 10 + *p - '0';
            len++;
        }
        sum = pow(sum, len * sign);

        return make_float_node(make_double_type(), sum);
    }

    return make_int_node(make_int_type(), (long)sum);
}

struct Node *read_hex(const char *number)
{
    long sum = 0;
    
    char *p = number;
    while(*p) {
        if(*p >= '0' && *p <= '9') {
            sum = sum * 10 + *p - '0';
        }else {
            sum = sum * 16 + tolower(*p) - 'a';
        }
    }

    struct Node *node = make_number_node(make_int_type());
    node->ival = sum;

    return node;
}

BOOL iskeyword(struct Token *tok, int c)
{
   return (tok->kind == KIND_KEYWORD) && (tok->id == c);
}

struct Node *read_number_node()
{
    struct Token *tok = get_token();
    if(tok->kind != KIND_NUMBER) {
        error_force("unexpected number!");
    }

    switch(tok->id) {

        //band 8
        case KIND_OCTAL:
            return read_octal(tok->token_string);

        //band 16
        case KIND_HEX:
            return read_hex(tok->token_string);
        default:
            break;       
    }

    return read_decimal(tok->token_string);
}

struct Node *read_char_node(int c)
{
    struct Node *node = make_node();
    node->ty = make_char_type();
    node->ival = c;

    return node;
}

struct Node *make_const_ident(struct TYPE *ty, const char *name)
{
    struct Node *node = make_node();

    node->ty = ty;
    node->ty->kind_qualiter = KEYWORD_CONST;

    node->kind = KIND_IDENT;
    node->varname = name;

    return node;
}

struct Node *make_binary_node(struct Node *left, struct Node *right, int id)
{
    struct Node *binary = make_node();
        
    binary->left = left;
    binary->right = right;
    binary->kind = AST_BINARY;

    binary->id = id;

    return binary;
}

struct Node *make_ident_node(struct TYPE *ty, const char *name)
{
    struct Node *node = make_node();
    node->kind = KIND_IDENT;
    node->strval = name;
    node->ty = ty;

   return node;
}

struct Node *make_struct_ref_node(struct Node *struc, const char *name)
{
    struct Node *node = make_node();
    node->kind = AST_STRUCT_REF;
    node->field = name;
    node->struc = struc;
    
    return node;
}

struct Node *make_cast_node(struct TYPE *ty, struct Node *operand)
{
    return make_unary_node(AST_CAST, ty, operand);
}

struct Node *make_conv_node(struct TYPE *ty, struct Node *operand)
{
    return make_unary_node(AST_CONV, ty, operand);
}

struct Node *make_case_node(int pos, char *label)
{
    struct Node *node = make_node();
    node->kind = KEYWORD_CASE;
    node->pos = pos;
    node->label = label;

    return node;
}

struct Node *make_compound_node(struct Buffer *list)
{
    struct Node *node = make_node();
    node->kind = AST_COMPOUND;
    node->lists = list;

    return node;
}

struct Node *make_if_node(struct Node *condition, struct Node *then, struct Node *els)
{
    struct Node *node = make_node();

    node->kind = AST_IF;

    node->condition = condition;

    node->then = then;
    node->els = els;

    return node;
}

struct Node *make_var_node(int kind, struct TYPE *ty, const char *varname)
{
    struct Node *node = make_node();

    node->kind = kind;
    node->ty = ty;
    node->varname = varname;

    return node;
}

struct Node *make_local_var_node(struct TYPE *ty, const char *varname)
{
    struct Node *node = make_var_node(AST_LOCAL_VAR, ty, varname);

    //重定义不允许
    if(local_env) {
        if(map_get(local_env, varname)) {
            error_force("redefine var name:%s", varname);
        }
        map_put(local_env, varname, node);
    }

    if(local_vars) {
        buffer_push(local_vars, node);
    }

    return node;
}

//FIXME: 静态的变量应该放在哪里??
struct Node *make_static_lvar_node(struct TYPE *ty, const char *varname)
{
    struct Node *node = make_var_node(AST_GLOBAL_VAR, ty, varname);

    if(local_env) {
        if(map_get(local_env, varname)) {
            error_force("redefine var name: %s", varname);
        }
        map_put(local_env, varname, node);
    }

    return node;
}

//FIXME: 全局变量需要放在哪里?
struct Node *make_global_var_node(struct TYPE *ty, const char *varname)
{
    struct Node *node = make_var_node(AST_GLOBAL_VAR, ty, varname);
    
    if(global_env) {

        if(map_get(global_env, varname)) {
            error_force("redefine var name: %s", varname);
        }

        map_put(global_env, varname, node);
    }

    return node;
}

struct Node *make_decl_node(struct Node *var)
{
    struct Node *node = make_node();
    node->kind = AST_DECL;
    node->varnode = var;

    return node;
}

struct Node *make_redefine_node(struct TYPE *ty, const char *name)
{
    struct Node *node = make_node();
    node->kind = AST_TYPEDEF;
    node->ty = ty;
    node->def_name = name;

    map_put(env(), name, node);

    return node;
}

struct Node *make_func_node(struct TYPE *ret, struct Buffer *params, struct Node *body)
{
    struct Node *node = make_node();
    node->kind = AST_FUNC_DEF;

    node->ty = ret;
    node->params = params;
    node->body = body;

    return node;
}

void *make_pair(void *first, void *second)
{
    void **pair = (void**)malloc(sizeof(void*) * 2);
    pair[0] = first;
    pair[1] = second;

    return pair;
}

struct Node *read_func_call(struct Node *func_node)
{
    struct Token *tok = peek_token();

    //只有函数和函数指针能使用()
    if(func_node->kind != AST_FUNC_DEF || func_node->kind != KIND_POINTER) {
        error_force("expect function but got %s\n", tok->token_string);
    }

    if(func_node->kind == KIND_POINTER && func_node->ty->kind != AST_FUNC_DEF) {
        error_force("expect function but got %s\n", tok->token_string);
    }

    int pos = 0;
    size_t len = buffer_len(func_node->params);

    if(len == 0) {
        expect(')');
        return func_node;
    }

    struct Node *node = NULL;
    void **param = NULL;
    struct Node *arg = NULL;

    for(;;) {

        if(next_token(')')) {
            break;
        }

        arg = read_assignment_expr();
        param = buffer_get(func_node->args, pos++);
        node = param[1];
    
        //假如形参与实参不一致，需要转换
        if(node->ty->kind != arg->ty->kind) {
            arg = make_cast_node(node->ty, arg);
        }

        buffer_push(func_node->args, arg);

        expect(',');
        continue;
    }

    //变参函数仅仅校验当前的参数是否大于等于实际参数
    if(func_node->ty->is_indeterminate) {
        if(buffer_len(func_node->ty->params) < buffer_len(func_node->args)){
            error_force("arg len is not match!");
        }
        return func_node;
    }

    //参数不一致，报错！
    if(buffer_len(func_node->ty->params) != buffer_len(func_node->args)){
        error_force("arg len is not match!");
    }

    return func_node;
}

struct Node *read_var_function(struct Token *tok)
{
    if(tok->kind != KIND_IDENT) {
        error_force("expect identify: but got :%s", tok->token_string);
    }

    struct Node *node = map_get(env(), tok->token_string);
    if(!node) {
        error_force("undefine identify: %s", tok->token_string);
    }

    return node;
}

struct Node *read_primary_exp()
{
    struct Token *tok = peek_token();
    switch (tok->kind)
    {
        case KIND_NUMBER:
            return read_number_node();
        case '(':  {
            struct Node *node = read_expr();
            expect(')');
            return node;
        }
        case KIND_IDENT:
            return read_var_function(tok);
        case KIND_CHAR:
            return read_char_node(tok->id);
        case KIND_STRING:
            return make_string_node(tok->token_string);

        case KIND_KEYWORD: {
            if(iskeyword(tok, '+') || iskeyword(tok, '-')) {
                struct Token *tok1 = peek_token();
                if(tok1->kind != KIND_NUMBER) {
                    error_force("expect number, but got: %s", tok1->token_string);
                }

                int sign = iskeyword(tok, '+') ? 1 : -1;

                struct Node *node = read_primary_exp();
                if(node->kind == AST_LITERAL) {
                    switch(node->ty->kind) {
                        case KIND_CHAR:
                        case KIND_INTEGER:
                        case KIND_SHORT: {
                            node->ival *= sign;
                            return node;
                        }
                       case KIND_FLOAT:
                       case KIND_DOUBLE: {
                           node->fval *= sign;
                           return node;
                       }
                    }
                }
            }
            unget_token(tok);
            return NULL;
        }
        default:
            break;
    }

    error_force("unexpected token:%s\n", tok->token_string);

    return NULL;
}

struct Node *read_array_expr(struct Node *node)
{
    if(next_token(']')) {
        error_force("array len should not zero!\n");
    }

    //函数中的计算值.
    struct Node *bin = make_binary_node(node, read_condition_expr(), '+');
    
    expect(']');

    return make_deref_node(node->ty->ptr, bin);
}

struct Node *read_struct_field(struct Node *node)
{
    if(node->kind != KIND_STRUCT && node->kind == KIND_POINTER && node->ty->kind != KIND_STRUCT) {
        error_force("expect an struct !\n");
    }

    struct Token *tok = get_token();
    if(tok->kind != KIND_IDENT) {
        error_force("error expect field, but got:%s!", tok->token_string);
    }

    char *name = tok->token_string;
    struct Node *field = map_get(node->fields, name);

    if(!field) {
        error_force("struct has not the field!");
    }

    return make_struct_ref_node(node, name);
}

struct Node *read_postfix_expr()
{
    struct Node *node = read_primary_exp();
    if(!node) {
        return NULL;
    }

    struct Token *tok = get_token();

    for(;;)
    {
        if(tok->id == OP_INC || tok->id == OP_DEC) {

            struct Node *operand = read_postfix_expr();
            return make_unary_node(tok->id, operand->ty,  operand);
        }

        if(tok->id == '[') {
            node = read_array_expr(node);
            continue;
        }

        if(tok->id == '(') {
            node = read_func_call(node);
            continue;
        }

        if(tok->id == OP_ARROW) {
           node = make_deref_node(node->ty->ptr, node);
           node = read_struct_field(node);
           continue;
        }

        if(tok->id == '.') {
           node = read_struct_field(node);
           continue;
       }

        break;
    }

    unget_token(tok);

    return node;
}

struct Node *read_bitnot()
{
    return make_unary_node(OP_NEG, NULL, read_cast_expr());
}

struct Node *read_log_not()
{
    return make_unary_node(OP_LOG_NOT, NULL, read_cast_expr());
}

struct Node *read_incdec_expr(int id)
{
    struct Token *tok = get_token();
    if(tok->kind != KIND_IDENT) {
        error_force("lvalue required as increment operand!");
    }

    struct Node *operand = read_cast_expr();

    return make_unary_node(id, operand->ty, operand);
}

//读取的是变量或者是一个类型
struct TYPE *read_cast_ident()
{
    struct Token *tok = peek_token();
    if(tok->kind == KIND_IDENT) {
        tok = get_token();
        struct Node *node = map_get(env(), tok->token_string);
        if(node) {
            return node->ty;
        }
        error_force("undefine identify: %s", tok->token_string);
    }

    char *vname = NULL;
    struct TYPE *base_ty = read_decl_specifier();

    if(!base_ty) {
        error_force("expect type, but got:%s", tok->token_string);
    }

    return read_declaretor(base_ty, &vname, DECL_CAST);
}

struct Node *read_sizeof_expr()
{
    expect('(');
    struct TYPE *ty = read_cast_ident();

    struct Node *node = make_ident_node(ty, make_temp_name());

    expect(')');

    expect(';');

    return make_unary_node(OP_SIZE_OF, NULL, node);
}

struct Node *read_addr_expr()
{
    struct Node *operand = read_cast_expr();
    return make_unary_node(AST_ADDR, operand->ty, operand);
}

struct Node *read_deref_expr()
{
    struct Node *operand = read_cast_expr();

    return make_unary_node(AST_DEREF, operand->ty, operand);
}

struct Node *read_unary_expr()
{
    struct Token *tok = get_token();
    if(tok->kind == KEYWORD_GOTO) {
        switch (tok->id)
        {
            case OP_INC:
            case OP_DEC:
                return read_incdec_expr(OP_PRE_DEC);
            case OP_NEG:
                return read_bitnot();
            case OP_LOG_NOT: 
                return read_log_not();
            case OP_SIZE_OF:
                return read_sizeof_expr();
            case '&':
                return read_addr_expr();
            case '*':
            return read_deref_expr();

        default:
            break;
        }
    }
    unget_token(tok);

    return read_postfix_expr();
}

//读取一个类型，注意和普通的声明分开，不能带有变量!
struct TYPE *read_cast_type()
{
    char *rname = NULL;
    struct TYPE *ty = read_decl_specifier();
    return read_declaretor(ty, &rname, DECL_CAST);
}

struct Node *read_cast_expr()
{
    if(expect('(')) {

        struct TYPE *ty = read_cast_type();

        expect(')');

        return make_cast_node(ty, read_cast_expr());
    }

    return read_unary_expr();
}

BOOL is_multi_operation(int id)
{
    switch (id)
    {
        case '*':
        case '/':
        case '%':
        return TRUE;
        
        default:
            return FALSE;
    }
}

struct Node *read_multiplicative_expr()
{
    struct Node *node = read_cast_expr();

    struct Token *tok = get_token();
    while(is_multi_operation(tok->id)) {

        struct Node *right = read_cast_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_additive_expr()
{
    struct Node *node = read_multiplicative_expr();

    struct Token *tok = get_token();
    if(tok->kind != KIND_KEYWORD) {
        unget_token(tok);

        return node;
    }

    while(iskeyword(tok, '+') || iskeyword(tok, '-')) {

        struct Node *right = read_multiplicative_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }

    unget_token(tok);

    return node;
}

struct Node *read_bitxor_expr()
{
    struct Node *node = read_additive_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, '^')) {

        struct Node *right = read_additive_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_bitand_expr()
{
    struct Node *node = read_bitxor_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, '&')) {

        struct Node *right = read_bitxor_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_bitor_expr()
{
    struct Node *node = read_bitand_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, '|')) {

        struct Node *right = read_bitand_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_shift_expr()
{
    struct Node *node = read_bitor_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, OP_SAL) || iskeyword(tok, OP_SAR)) {

        struct Node *right = read_bitor_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

BOOL is_relation_op(int id)
{
    switch(id) {
        case '>':
        case '<':
        case OP_BIG_QUE:
        case OP_LITTLE_EQU:
        case OP_EQU:
            return TRUE;
    }

    return FALSE;
}

struct Node *read_relational_expr()
{
    struct Node *node = read_shift_expr();

    struct Token *tok = get_token();
    if(tok->kind != KIND_KEYWORD) {
        return node;
    }

    while(is_relation_op(tok->id)) {
        struct Node *right = read_shift_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_log_and_expr()
{
    struct Node *node = read_relational_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, OP_LOG_AND)) {

        struct Node *right = read_relational_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_log_or_expr()
{
    struct Node *node = read_log_and_expr();

    struct Token *tok = get_token();
    while(iskeyword(tok, OP_LOG_OR)) {

        struct Node *right = read_log_and_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_condition_expr()
{
    struct Node *node = read_log_or_expr();
    if(!next_token('?')) {
        return node;
    }

    struct Node *then = read_expr();
    expect(':');

    struct Node *els = read_log_or_expr();
    expect(';');

    return make_if_node(node, then, els);
}

struct Node *read_if_expr()
{
    struct Node *condition = read_condition_expr();

    if(next_token(';')) {
        return make_if_node(condition, NULL, NULL);
    }

    struct Node *then = read_expr();

    struct Node *els = NULL;
    if(next_token(KEYWORD_ELSE)) {
        els = read_expr();
    }

    return make_if_node(condition, then, els);
}

struct Node *read_boolean_expr()
{
    struct Node *node = read_expr();
    if(!node) {
        error_force("expect a expression, but got null");
    }

    //TODO:浮点型需要转换吗
    if(node->ty->kind == KIND_FLOAT) {
        return FALSE;
    }

    return node;
}

//SET JUMP LABEL
#define SET_JUMP(contin, brk) \
    char *old_continue = long_continue; \
    char *old_break = long_break; \
    long_continue = contin; \
    long_break = brk; \

//RESTORE JUMP LABEL
#define RESTORE_JUMP() \
    long_continue = old_continue; \
    long_break = old_break; \

struct Node *read_while_expr()
{
    char *begin = make_label();
    char *end = make_label();

    expect('(');

    struct Buffer *list = make_buffer();

    struct Node *condition = read_boolean_expr();
    buffer_push(list, make_if_node(condition, NULL, make_jump_node(end)));

    expect(')');

    SET_JUMP(begin, end);

    buffer_push(list, make_label_node(begin));

    struct Node *body = read_stmt_expr();

    RESTORE_JUMP();

    if(body) {
        buffer_push(list, body);
    }

    buffer_push(list, make_label_node(end));

    return make_compound_node(list);
}

struct Node *read_decl_stmt_opt()
{
    if(next_token(';')) {
        return NULL;
    }

    return read_decl_stmt();
}

struct Node *read_for_expr()
{
    expect('(');

    char *begin = make_label();
    char *mid = make_label();
    char *end = make_label();

    struct Map *old_env = local_env;
    set_env(make_map_parent(local_env));

    //for 的初始化语句需要一个局部环境。
    struct Map *sub_env = local_env;
    set_env(make_map_parent(local_env));

    struct Node *init = read_decl_stmt_opt();

    set_env(sub_env);

    expect(';');

    struct Node *condition = read_expr();

    condition = make_if_node(condition, NULL, make_jump_node(end));

    expect(';');

    struct Node *step = read_expr();

    expect(')');

    SET_JUMP(mid, end)

    struct Node *body = read_expr();

    RESTORE_JUMP()

    set_env(old_env);

    struct Buffer *list = make_buffer();
    if(init) {
        buffer_push(list, init);
    }

    buffer_push(list, make_label_node(begin));

    if(condition) {
        buffer_push(list, condition);
    }

    if(body) {
        buffer_push(list, body);
    }

    buffer_push(list, make_label_node(mid));

    if(step) {
        buffer_push(list, step);
    }

    buffer_push(list, make_jump_node(begin));

    buffer_push(list, make_label_node(end));

    return make_compound_node(list);
}

struct Node *read_do_while_expr()
{
    char *begin = make_label();
    char *end = make_label();

    struct Buffer *list = make_buffer();

    SET_JUMP(begin, end)

    struct Node *body = read_stmt_expr();

    RESTORE_JUMP()

    expect(KEYWORD_WHILE);

    expect('(');

    struct Node *condit = read_boolean_expr();

    expect(')');
    expect(';');

    buffer_push(list, make_label_node(begin));

    if(body) {
        buffer_push(list, body);
    }

    buffer_push(list, make_if_node(condit, make_jump_node(begin), NULL));

    buffer_push(list, make_label_node(end));

    return make_compound_node(list);
}

struct Node *read_label_expr(const char *label)
{
    struct Buffer *list = make_buffer();

    buffer_push(list, make_label_node(label));

    struct Node *node = read_stmt_expr();
    if(node) {
        buffer_push(list, node);
    }

    return make_compound_node(list);
}

//FIXME: 注意环境问题.
struct Node *read_compound_expr()
{
    struct Map *current = local_env;
    local_env = make_map_parent(current);

    struct Buffer *list = make_buffer();

    for(;;) {
        buffer_push(list, read_decl_stmt());
        if(next_token('}')) {
            break;
        }
    }

    local_env = current;

    return make_compound_node(list);
}

struct Node *read_break_expr()
{
    if(!long_break) {
        error_force("undefine label!");
    }

    struct Node *node = make_jump_node(long_break);

    expect(';');

    return node;
}

struct Node *read_continue_expr()
{
    if(!long_continue) {
        error_force("undefine continue!");
    }

    struct Node *node = make_jump_node(long_continue);

    expect(';');

    return node;
}

struct Node *read_case_expr()
{
    int pos = read_interpreter_expr();
    expect(':');

    char *label = make_label();
    struct Node *case_node = make_case_node(pos, label);

    buffer_push(cases, case_node);

    struct Node *node = read_stmt_expr();

    struct Buffer *list = make_buffer();
    buffer_push(list, make_label_node(label));
    buffer_push(list, node);

    return make_compound_node(list);
}

struct Node *make_case_jump(struct Node *var, struct Node *case_node)
{
    struct Node *t = make_ident_node(make_int_type(), make_temp_name());

    struct Node *node = make_binary_node(var, t, OP_EQU);

    return make_if_node(node, make_jump_node(case_node->label), NULL);
}

#define SET_SWITCH(brk) \
    struct Buffer *old_cases = cases; \
    char *old_default_case = default_case; \
    char *old_long_break = long_break; \
    default_case = NULL; \
    cases = make_buffer(); \
    long_break = brk;

#define RESTORE_SWITCH() \
    cases = old_cases; \
    default_case = old_default_case; \
    long_break = old_long_break; \

struct Node *read_switch_expr()
{
    expect('(');
    struct Node *var = read_condition_expr();
    expect(')');

    char *end = make_label();

    struct Buffer *list = make_buffer();

    SET_SWITCH(end)
    struct Node *body = read_stmt_expr();

    size_t len = buffer_len(cases);
    for(size_t i = 0; i < len; i++) {
        buffer_push(list, make_case_jump(var, buffer_get(cases, i)));
    }

    char *default_label = default_case ? default_case : end;
    buffer_push(list, make_jump_node(default_label));

    if(body) {
        buffer_push(list, body);
    }

    buffer_push(list, make_label_node(end));

    RESTORE_SWITCH()

    return make_compound_node(list);
}

struct Node *read_return_expr()
{
    struct Node *operand = NULL;

    if(!func_type) {
        expect(';');
        return make_unary_node(AST_RETURN, NULL, NULL);
    }

    //TODO: 返回类型校验是否与函数的返回类型一致.
    operand = read_condition_expr();
    if(!operand) {
        error_force("expect return expression!");
    }

    expect(';');

    return make_unary_node(AST_RETURN, NULL, operand);
}

struct Node *read_stmt_expr()
{
    struct Token *tok = get_token();

    if(tok->kind == KIND_KEYWORD) {
        switch(tok->id) {
            case KEYWORD_GOTO: 
                return read_goto_expr();
            case KEYWORD_IF:
                return read_if_expr();
            case KEYWORD_WHILE:
                return read_while_expr();
            case KEYWORD_DO:
                return read_do_while_expr();
            case KEYWORD_FOR:
                return read_for_expr();
            case KEYWORD_SWITCH:
                return read_switch_expr();
            case KEYWORD_RETURN: 
                return read_return_expr();
            case '{':
                return read_compound_expr();
            case KEYWORD_CONTINUE:
                return read_continue_expr();
            case KEYWORD_BREAK:
                return read_break_expr();
            case KEYWORD_CASE:
                return read_case_expr();
            default:
                break;
        }
    }

    if(tok->kind == KIND_IDENT) {
        expect(':');
        return read_label_expr(tok->token_string);
    }

    unget_token(tok);

    return read_expr();
}

struct Node *read_goto_expr()
{
    struct Token *tok = get_token();
    if(tok->kind != KIND_IDENT) {
        error_force("unexpected keyword!\n");
    }
    expect(':');
    expect(';');

    if(!map_get(local_env, tok->token_string)) {
        error_force("label %s used but not defined", tok->token_string);
    }

    return make_jump_node(tok->token_string);
}

//注意, 跟其他的二元表达式不一样，这里是右结合.
struct Node *read_assignment_expr()
{
    struct Node *node = read_condition_expr();

    struct Token *tok = get_token();

    while(iskeyword(tok, '=')) {

        struct Node *right = read_assignment_expr();
        node = make_binary_node(node, right, tok->id);

        tok = get_token();
    }
    unget_token(tok);

    return node;
}

struct Node *read_expr()
{
    struct Buffer *list = make_buffer();

    struct Node *node = read_assignment_expr();

    buffer_push(list, node);

    struct Token *tok = get_token();
    while(iskeyword(tok, ',')) {

        node = read_assignment_expr();
        buffer_push(list, node);

        tok = get_token();
    }
    unget_token(tok);

    expect(';');

    return make_compound_node(list);
}

BOOL istype(int kind)
{
   switch(kind) {
        case KIND_ARRAY:
        case KIND_CHAR:
        case KIND_ENUM:
        case KIND_POINTER:
        case KIND_STRING:
        case KIND_STRUCT:
        case KIND_FUNC:
        case KIND_TYPEDEF:
            return TRUE;
    }

    return FALSE;
}

BOOL isfunc()
{
    struct Buffer *buf = make_buffer();

    struct Token *tok = NULL;

    BOOL state = FALSE;
    for(;;) {
        tok = get_token();
        buffer_push(buf, tok);
    
        if(iskeyword(tok, '(')) {
            if(next_token('{')) {
                state = TRUE;
            }
        }
    }
    state = FALSE;

    size_t len = buffer_len(buf);
    while(len--) {
        unget_token(buffer_pop(buf));
    }

    return state;
}

BOOL isstorage(int id)
{
   return (id >= KEYWORD_REGISTER && id <= KEYWORD_AUTO);
}

int eval(struct Node *node)
{
    if(!node) {
        return 0;
    }

    //可以是ident, 但必须是常量
    if(node->kind == KIND_IDENT) {
        if(node->ty->kind != KIND_INTEGER || node->ty->kind_qualiter != KEYWORD_CONST) {
            error_force("expect integer expression!");
        }
    }

    if(node->kind == KIND_NUMBER) {
        if(node->ty->kind != KIND_INTEGER) {
            error_force("expect integer expression!");
        }
    }

    if(node->kind == KIND_CHAR) {
        return node->ival;
    }

    //只有! 和 sizeof 形式的一元表达式有效.
    if(node->kind == AST_UNARY) {
        switch (node->id) {
            case OP_SIZE_OF:
                return node->operand->ty->kind;
            case '!':
                return !eval(node->operand);
            default:
                error_force("expect integer expression!");
        }
    }

    if(node->kind == AST_IF) {
        if(!node->condition) {
            return 0;
        }
        int condition = eval(node->condition);
        
        if(condition) {
            if(!node->then) {
                if(!node->els) {
                    return 0;
                }
            }
            return eval(node->then);
        } else {
            if(!node->els) {
                return 0;
            }

            return eval(node->then);
        }

    }

    if(node->kind == AST_BINARY) {
        switch (node->id) {
            case '-':
                return eval(node->left) - eval(node->right);
            case '+':
                return eval(node->left) + eval(node->right);
            case '/':
                return eval(node->left) / eval(node->right);
            case '*':
                return eval(node->left) * eval(node->right);
            case '%':
                return eval(node->left) % eval(node->right);
            case '~':
                return ~eval(node->operand);
            case '|':
                return eval(node->left) | eval(node->right);
            case '^':
                return eval(node->left) ^ eval(node->right);
            case '&':
                return eval(node->left) & eval(node->right);
            case '!':
                return !eval(node->operand);
            case OP_SAL:
                return eval(node->left) << eval(node->right);
            case OP_SAR:
                return eval(node->left) << eval(node->right);
            case OP_LOG_OR:
                return eval(node->left) || eval(node->right);
            case OP_LOG_AND:
                return eval(node->left) && eval(node->right);
        }
    }

    error_force("expect const expression!");

    return 0;
}

int read_interpreter_expr()
{
    return eval(read_condition_expr());
}

struct TYPE *read_declare_array(struct TYPE *base_ty) {

    int len = 0;
    if(next_token(']')) {
        //这个是一个自动推导长度的声明方式
        len = -1;
    } else {

        //这个是一个整形常量表达式的声明方式
        len = read_interpreter_expr();
        if(len < 1) {
            error_force("array's len wrong");
        }
        expect(']');
    }
    
    //多维数组的声明
    struct TYPE *ty = read_declaretor_tail(base_ty, DECL_DEFAULT);

    //类型不能是函数数组
    if(ty->kind == KIND_FUNC) {
        error_force("error array of function!\n");
    }

    return make_array_type(ty, len);
}

struct TYPE *read_func_params(struct TYPE *ret, int state)
{
    //没有参数的函数声明
    if(next_token(')')) {
        return make_func_type(ret, NULL);
    }

    struct Token *tok = get_token();
    if(iskeyword(tok, KEYWORD_VOID) && next_token(')')) {
        return make_func_type(ret, NULL);
    }
    unget_token(tok);

    struct Buffer *params = make_buffer();

    struct TYPE *baste_ty = NULL;
    struct TYPE *ty = NULL;
    char *param_name = NULL;

    struct Node *node = NULL;
    for(;;) {

        tok = peek_token();
        //假如是省略号
        if(iskeyword(tok, KEYWORD_ELLIPSIS)) {
            if(buffer_len(params) == 0) {
                error_force("ellipsis should not the first params");
            }

            tok = get_token();
            ret->is_indeterminate = TRUE;

            //变参只能在参数的结尾.
            expect(')');

            return make_func_type(ret, params);
        }

        baste_ty = read_decl_specifier();
        if(!baste_ty) {
            error_force("expect params but got:%s", tok->token_string);
        }

        //函数参数，不能是typedef, static, extern, 只能是register
        //不能进行初始化, 意味着参数后面只能是, 或者)
        if(baste_ty->storage == KIND_TYPEDEF || baste_ty->storage == KIND_STATIC ||   \
            baste_ty->storage == KIND_EXTERN) {
            error_force("function declare can not use storage, unless register");
        }

        //参数名字可要可不要
        ty = read_declaretor(baste_ty, &param_name, DECL_PARAM);

        //函数的参数类型是函数或者是数组的时候转换为指针.
        if(ty->kind == KIND_FUNC || ty->kind == KIND_ARRAY) {
            ty = make_ptr_type(ty);
        }

        //没有名字的时候，参数只作为占位的作用，不能访问.
        if(!param_name) {
            //没有参数名，则创建一个临时的参数名进去
            node = make_local_var_node(ty, make_temp_name());
            buffer_push(params, make_pair(NULL, node));
        } else {
            node = make_local_var_node(ty, param_name);
            buffer_push(params, make_pair(param_name, node));
        }

        if(next_token(',')) {
            continue;
        }

        if(next_token(')')) {
            break;
        }
    }

    return make_func_type(ret, params);
}

struct TYPE *read_declare_func(struct TYPE *ret, int state)
{
    //不能从函数中返回一个函数
    if(ret->kind == KIND_FUNC) {
        error_force("function returning a function!");
    }

    //语义不对，不管是函数数组还是数组函数都是不合法的.
    if(ret->kind == KIND_ARRAY) {
        error_force("array of functions!");
    }

    return read_func_params(ret, state);
}

//C语言的声明存在着优先级，因此需要分三部分来读取.
struct TYPE *read_declaretor_tail(struct TYPE *base_ty, int state)
{
    //假如是[, 那么这是一个数组声明。
    if(next_token('[')) {
        return read_declare_array(base_ty);
    }

    //假如是( 那么这个是一个函数声明
    if(next_token('(')) {
        return read_declare_func(base_ty, state);
    }

    return base_ty;
}

int read_bit_field(char *name, struct TYPE *ty)
{
    int len = read_interpreter_expr();
    int max = ty->size * 8;

    if(len < 0 || len > max) {
        error_force("invalid bit size!");
    }

    if(len == 0 && name == NULL) {
        error_force("un name bit field must be not zero!");
    }

    return len;
}

//假如成员是结构体或者共用体，则成员必须是完整的类型，不能是单独是声明。
struct TYPE *read_struct_union_def(struct TYPE **type_ptr, struct Buffer *fields)
{
    struct Token *tok = peek_token();
    if(tok->kind == KIND_IDENT) {

        tok = read_token();
        struct TYPE *tk = (struct TYPE *)is_in_tags(tok->token_string);

        //当前读到的类型和之前保存的类型不一致
        if(tk && tk->kind != tok->kind) {
            error_force("type is not match!");
        }

        (*type_ptr)->type_name = tok->token_string;
        put_tag(tok->token_string, *type_ptr);
    }

    //不是结构体或者联合的定义，返回
    if(!next_token('{')) {
        return *type_ptr;
    }

    struct TYPE *base_ty = NULL;
    struct TYPE *ty = NULL;
    char *field_name = NULL;

    for(;;) {

        tok = peek_token();
        base_ty = read_decl_specifier();
        if(!base_ty) {
            error_force("type expect, but got: %s", tok->token_string);
        }

        //空定义
        if(base_ty->kind == KIND_STRUCT || base_ty->kind == KIND_UNION) {
            if(next_token(';')) {
                buffer_push(fields, make_pair(NULL, ty));
                continue;
            }
        }
        
        for(;;) {

            ty = read_declaretor(base_ty, &field_name, DECL_PARAM);

            if(!field_name) {
                error_force("expect identify!");
            }

            if(next_token(':')) {
                ty->bitsize = read_bit_field(field_name, base_ty);
            }

            buffer_push(fields, make_pair(field_name, ty));

            //TODO: 需要把类型是函数的成员转成函数指针

            //读完后，开始校验类型是否合法。
            if(base_ty->kind == KEYWORD_STRUCT || base_ty->kind == KEYWORD_UNION) {
               struct TYPE *tk = (struct TYPE *)is_in_tags(base_ty->type_name);
                if(tk->size == -1) {
                    error_force("incomplete type: %s", base_ty->type_name);
                }
            }

            if(next_token(',')) {
                continue;
            }

            if(next_token(';')) {
                break;
            }
        }
        if(next_token('}')) {
            break;
        }
    }

    return *type_ptr;
}

BOOL check_struct_union_def(struct TYPE *ty, struct Buffer *fields)
{
    if(ty->type_name == NULL || ty->size == -1) {
        return TRUE;
    }

    size_t len = buffer_len(fields);

    void **pair = NULL;
    char *name = NULL;
    struct TYPE *field = NULL;

    size_t i = 0;
    for(; i < len; i++) {

        pair = buffer_get(fields, i);
        name = pair[0], field = pair[1];

        if(field->kind == KIND_ARRAY) {
            if(ty->len == -1) {
                error_force("array should not the only one array!");
            }

           if(i != len - 1) {
                error_force("array must be the last one elem");
           }
       }
   }

    return TRUE;
}

void adjust_uname_struct(struct Dict *r, struct TYPE *uname, int offset)
{
    struct Buffer *fields = dict_keys(uname->fields);

    size_t i = 0;
    size_t len = buffer_len(fields);

    char *name = NULL;
    struct TYPE *field = NULL;
    for(; i < len; i++) {

        name = buffer_get(fields, i);
        field = dict_get(uname->fields, name);

        //设置位置为结构体的偏移首地址.
        field->offset += offset;

        if(!name) {
            name = make_temp_name();
        }
        dict_put(r, name, field);
    }
}

int calc_bitalign(int off, int bitoff)
{
    //一个字节8个位, 因此我们假如超过了8个位，则需要补充指定的字节。
    return (bitoff + 7) / 8;
}

size_t calc_fieldalign(int offset, size_t align)
{
    if(offset % align == 0) {
        return 0;
    }
    
    // n = k * m + C
    // m(k + 1) = n - (n % k) + m
    return offset - (offset % align) + align;
}

struct TYPE *padding_struct(struct TYPE *ty, struct Buffer *fields)
{
    size_t len = buffer_len(fields);
    size_t i = 0;

    size_t off = 0;
    size_t bitoff = 0;
    int align = 1;

    void **pair = NULL;

    char *name = NULL;
    struct TYPE *field = NULL;

    size_t remain = 0;

    for(; i < len; i++) {

        pair = buffer_get(fields, i);
        name = pair[0];
        field = pair[1];

        align = max(align, field->align);

        //假如是空的结构体，需要把结构体的字段拷贝出外层来，并且按照结构体的大小调整空间。
        if(name == NULL && field->kind == KEYWORD_STRUCT) {

            off += calc_bitalign(off, bitoff);
            bitoff = 0;

            off += calc_fieldalign(off, field->align);
            adjust_uname_struct(ty->fields, field, off);

            off += field->size;

            continue;
        }

        if(name) {
            dict_put(ty->fields, name, field);
        }

        //当前的字段是位段
        if(field->bitsize > 0) {

            //当前的剩余位段空间足够
            if(field->bitsize <= remain) {
                field->offset = off;
                field->bitoffset = bitoff;

                remain -= field->bitsize;
            }

            //位段空间不够，申请新的空间，并且调过上一个的位段空间。
            if(field->bitsize > remain) {
    
                off += calc_bitalign(off, bitoff);

                field->offset = off;
                field->bitoffset = 0;
                bitoff = 0;

                remain = field->size * 8;
                remain -= field->bitsize;
            }
            
            bitoff += field->bitsize;

            continue;
        }

        //位段为0, 合并上一个字节.
        if(field->bitsize == 0) {
            off += calc_bitalign(off, bitoff);
            bitoff = 0;

            off += calc_fieldalign(off, field->align);
            continue;
        }

        //当前不是位段。
        if(field->bitsize < 0) {

            //合并上一次的位段的空间.
            off += calc_bitalign(off, bitoff);
            bitoff = 0;

            off += calc_fieldalign(off, field->align);
            field->offset = off;
            off += field->size;
        }
    }

    ty->align = align;
    ty->size = calc_fieldalign(off, align);

    return ty;
}

struct TYPE *padding_union(struct TYPE *ty, struct Buffer *fields)
{
    size_t len = buffer_len(fields);
    size_t i = 0;

    int align = 1;
    int max_size = 0;

    void **pair = NULL;

    struct TYPE *field = NULL;
    char *name = NULL;
    for(; i < len; i++) {

        pair = buffer_get(fields, i);

        name = pair[0];
        field = pair[1];

        if(name == NULL && field->kind == KEYWORD_STRUCT) {
            adjust_uname_struct(ty->fields, field, 0);
        }

        align = max(align, field->align);
        max_size = max(max_size, field->size);

        //空的结构体是无法访问的，只占空间。
        if(name != NULL) {
            dict_put(ty->fields, name, field);
        }
    }

    ty->align = align;
    ty->size = calc_fieldalign(max_size, align);

    return ty;
}

struct TYPE *read_struct_def()
{
    struct TYPE *struct_ty = make_struct_type();

    struct Buffer *fields = make_buffer();
    struct_ty = read_struct_union_def(&struct_ty, fields);
    check_struct_union_def(struct_ty, fields);

    return padding_struct(struct_ty, fields);
}

struct TYPE *read_union_def()
{
    struct TYPE *union_ty = make_union_type();

    struct Buffer *fields = make_buffer();
    union_ty = read_struct_union_def(&union_ty, fields);
    check_struct_union_def(union_ty, fields);
    
    return padding_union(union_ty, fields);
}

struct TYPE *read_enum_def()
{
    struct TYPE *enum_ty = make_enum_type();

    return read_enum_fields(&enum_ty);
}

struct TYPE *read_enum_fields(struct TYPE **type_ptr)
{
    struct Token *tok = peek_token();

    if(tok->kind == KIND_IDENT) {
        tok = read_token();
        struct TYPE *tk = (struct TYPE *)is_in_tags(tok->token_string);

        //类型与之前声明的不符
        if(tk && tk->kind != tok->kind) {
            error_force("enum type is not match!");
        }

        (*type_ptr)->type_name = tok->token_string;
        put_tag(tok->token_string, *type_ptr);
    }

    //不是完整的枚举定义，跳过
    if(!next_token('{')) {
        return *type_ptr;
    }
    
    tok = NULL;

    int c = 0;
    for(;;) {

        tok = get_token();

        if(tok->kind != KIND_IDENT) {
            error_force("expect ident but got: %s", tok->token_string);
        }

        if(next_token('=')) {
            c = read_interpreter_expr();

            struct Node *node = make_const_ident(make_int_type(), tok->token_string);
            node->ival = c;

            map_put(env(), tok->token_string, node);
        }
        c++;

        if(next_token(',')) {
            continue;
        }

        if(next_token('}')) {
            break;
        }
    }

    return *type_ptr;
}

BOOL is_kind_qualifier(struct Token *tok)
{
    if(tok->kind != KIND_KEYWORD) {
        return FALSE;
    }

    switch (tok->id)
    {
        case KEYWORD_CONST:
        case KEYWORD_VOLATILE:
        case KEYWORD_AUTO:
        case KEYWORD_RESTRICT:
            return TRUE;
    default:
        break;
    }

    return FALSE;
}

BOOL is_kind_specifier(struct Token *tok)
{
    if(tok->kind == KIND_STRING) {
        return TRUE;
    }

    if(tok->kind == KIND_KEYWORD) {
        switch(tok->id) {
            case KEYWORD_UNION:
            case KEYWORD_STRUCT:
            case KEYWORD_ENUM:
            case KEYWORD_FLOAT:
            case KEYWORD_INT:
            case KEYWORD_CHAR:
            case KEYWORD_SHORT:
            case KEYWORD_DOUBLE:
            case KEYWORD_LONG:
                return TRUE;
        }
    }

    return FALSE;
}

struct TYPE *read_decl_specifier()
{
    struct Token *tok = NULL;

    struct TYPE *ty = make_type();

    int sig = 0;

    for(;;) {

        tok = get_token();

        //假如是; ，那么意味着声明结束。
        if(tok->id == ';') {

            //前面有类型限定符 或者 存储类型!，但是后面没有实际的类型，不允许!
            if(ty->kind == 0 && (ty->kind_qualiter != 0 || ty->storage != 0)) {
                error_force("error, unexpected kind_qualiter!\n");
            }

            return ty;
        }

        //存储类型的符号.
        if(iskeyword(tok, KEYWORD_SIGNED) || iskeyword(tok, KEYWORD_UNSIGNED)) {
            if(sig != 0) {
                error_force("duplicate signed!");
            }
            sig = tok->id;
            continue;
        }

        if(tok->kind == KIND_IDENT) {

            struct Node *node = map_get(env(), tok->token_string);
            if(node && node->kind == KEYWORD_TYPEDEF) {
                return node->ty;
            }

            //不允许没有类型的变量声明。
            if(ty->kind == 0) {
                error_force("unexpected type decl\n");
            }

            //void float double 等类型，不允许出现signed 或者 unsigned
            if(iskeyword(tok, KEYWORD_FLOAT) || iskeyword(tok, KEYWORD_DOUBLE)|| iskeyword(tok, KEYWORD_VOID)) {
                if(sig != 0) {
                    error_force("unexpected signed or unsigned\n");
                }
            }
            
            //标记是否是无符号
            ty->is_unsigned = sig == KEYWORD_UNSIGNED;

            unget_token(tok);

            return ty;
        }

        if(tok->kind == KIND_KEYWORD && isstorage(tok->id)) {

            //void extern typedef static只能 出现一次!
            if(ty->storage) {
                error_force("unexpected sotrage decl\n");
            }

            //void extern typedef static只能 出现在声明的最前面.
            if(ty->kind != 0 || ty->kind_qualiter != 0 ) {
                error_force("unexpected sotrage decl\n");
            }

            //保存当前的存储类型!
            ty->storage == tok->kind;

            continue;
        }

        if(tok->kind == KIND_KEYWORD && is_kind_qualifier(tok))
        {
            //同一个变量声明中，所有的类型限定符以第一个为准，后面的直接忽略!
            //保存当前的类型限定!
            if(ty->kind_qualiter == 0 ) {
                ty->kind_qualiter = tok->kind;
            }

            while(tok->kind == KIND_KEYWORD && is_kind_qualifier(tok)) {
                tok = get_token();
            }

            continue;
        }

        //类型读出来后，直接返回.
        if(is_kind_specifier(tok))
        {
            //只能出现一次类型声明!
            if(ty->kind != 0) {
                error_force("more than one times type speciter\n");
            }

            ty->kind = tok->kind;
            return ty;
        }

        //假如是结构体、共用体、枚举的时候需要获取完整的类型。
        if(tok->kind == KIND_KEYWORD) {

            switch(tok->id) {
                case KEYWORD_STRUCT:
                    return read_struct_def();
                case KEYWORD_ENUM:
                    return read_enum_def();
                case KEYWORD_UNION:
                    return read_union_def();
                default:
                    break;
            }
        }

        //以上都不是的话，肯定是出错了
        error_force("error unexpected keyword!\n");
    }
    
    return ty;
}

//state 用来标识是否需要读取名字
struct TYPE *read_declaretor(struct TYPE *base_ty, char **ident_name, int state)
{
    if(next_token('(')) {

        //括号里面的话，需要递归的读完整才行，因为括号内部需要作为一个整体和右边的结合
        //因此定义一个临时的类型作为这个类型的整体，把左边的基础类型和临时类型的右边才是这个完整的类型
        //等到读完后才把这个临时的类型的值修改为读完整后的类型
        struct TYPE *ty_temp = make_temp_type();
        struct TYPE *t = read_declaretor(ty_temp, ident_name, state);

        expect(')');

        //完整的类型在此才读完, 把临时的类型改为实际的类型.
        *ty_temp = *read_declaretor_tail(base_ty, state);

        //把括号最内层的类型返回最上层
        return t;
    }

    if(next_token('*')) {
        return read_declaretor_tail(make_ptr_type(base_ty), state);
    }

    struct Token *tok = get_token();

    //跳过所有的类型限定符，存在多个的时候以第一个为准.
    if(is_kind_qualifier(tok)) {

        base_ty->kind_qualiter = tok->kind;

        while(is_kind_qualifier(tok)) {
            tok = get_token();
        }
        unget_token(tok);
    }

    if(tok->kind == KIND_IDENT) {

        *ident_name = tok->token_string;

        //假如是普通的类型读取，强制转换等等， 则不允许读到标记符
        if(state == DECL_CAST) {
            error_force("unexpected identify!");
        }

        return read_declaretor_tail(base_ty, state);
    }

    //默认必须读到变量名
    if(state == DECL_DEFAULT) {
        error_force("expect identify, but got:%s", tok->token_string);
    }

    //声明到此为止, 读到的内容不是('(', '*', 名字, 类型限定符)) 那表示读完了, 把读多的东西放回去.
    unget_token(tok);

    //形参或者是结构体、联合、位段名字可要可不要.
    return base_ty;
}

//TODO: c99 has not finshed!
struct Buffer *read_array_init(struct TYPE *ty, struct Buffer *list, int off, int designated)
{

    BOOL has_brace = FALSE;
    if(next_token('{')) {
        has_brace = TRUE;
    }

    struct Token *tok = NULL;
    size_t i = 0;
    size_t pos;

    //FIXME: 有可能会读到很多数据.
    for(;;) {

        tok = get_token();

        //假如是遇到 }, 那么可能是当前的 }， 也有可能是上一层的.
        if(iskeyword(tok, '}')) {
            if(!has_brace) {
                unget_token(tok);
            }
        }

        //假如遇到的是[ 或者 . 但是当前不是指定初始化，而且不是被{} 包含的话， 那么意味着读到上一层的数据了
        //出现[ 有可能是, 上一层是一个数组，因此这里是读到上一层的指定初始化了
        //读到. 的时候，意味着可能是访问的 结构体的成员是数组，并且采用指定初始化的方式，因此 . 也有可能是结构体的下一个成员的
        // 初始化过程， 也要判断并且返回
        if((iskeyword(tok, '[') || iskeyword(tok, '.')) && !has_brace && !designated) {
            unget_token(tok);
            return;
        }

        if(iskeyword(tok, '[')) {

            //这里仅仅是常量表达式指定初始化，不支持指定范围初始化
            int idx = read_interpreter_expr();
            if(idx <= 0 || idx > ty->len) {
                error_force("out of range of array!");
            }

            i = idx;

            expect(']');

            designated = TRUE;
        }

        //read_init_list();
        

        //有可能是, 读完它
        next_token(',');

        designated = FALSE;

        ++i, ++pos;

    }

    return list;
}

struct Buffer *read_struct_init(struct TYPE *ty, struct Buffer *list)
{
    struct Token *tok = NULL;

    for(;;) {

        if(next_token('}')) {
            break;
        }

        if(next_token('.')) {
            tok = get_token();
            if(tok->kind != KIND_IDENT) {
                error_force("expect ident but got :%s", tok->token_string);
            }

            if(ty->fields) {
                if(!dict_get(ty->fields, tok->token_string)) {
                    error_force("struct has not %s field!", tok->token_string);
                }
            }

            buffer_push(list, read_decl_init(ty));
        }

        if(next_token(',')) {
            continue;
        }
    }

    return list;
}

struct Buffer *read_init_list(struct TYPE *ty, struct Buffer *list)
{
    if(ty->kind == KIND_ARRAY) {
        return read_array_init(ty, list);
    }

    if(ty->kind == KIND_STRUCT) {
        return read_struct_init(ty, list);
    }

    return list;
}

struct Buffer *read_decl_init(struct TYPE *ty)
{
    struct Buffer *init_list = make_buffer();
    if(next_token('{')) {
        read_init_list(ty, init_list);
        expect('}');

        return init_list;
    }

    buffer_push(init_list, read_assignment_expr());

    return init_list;
}

//是一条单独的声明语句或者单条的表达式语句.
struct Node *read_decl_stmt()
{
    struct Token *tok = peek_token();

    struct Node *node = NULL;
    if(istype(tok->kind)) {

        node = read_decl(FALSE);
        expect(';');

        return node;
    }

    return read_stmt_expr();
}

struct Node *read_func_body(const char *fname)
{
    local_env = make_map_parent(NULL);
    local_vars = make_buffer();

    local_labels = make_map();

    //把函数名加入到局部环境中.
    map_put(local_env, "__func__", make_string_node(fname));
    map_put(local_env, "__FUNC__", make_string_node(fname));

    expect('{');

    struct Node *body = read_compound_expr();

    local_vars = NULL;
    local_env = NULL;

    local_labels = NULL;

    return body;
}

void check_params(struct Buffer *params1, struct Buffer *params2)
{
    //参数个数不一致
    if(buffer_len(params1) != buffer_len(params2)) {
        error_force("function param len un match !");
    }

    size_t len = buffer_len(params1);
    size_t i = 0;

    void **param1 = NULL;
    void **param2 = NULL;

    struct TYPE *type1 = NULL;
    struct TYPE *type2 = NULL;
    for(; i < len; i++) {

        param1 = buffer_get(params1, i);
        type1 = param1[1];
    
        param2 = buffer_get(params2, i);
        type2 = param2[1];

        //声明的参数类型与定义的函数参数不一致.
        if(type1->kind != type2->kind) {
            error_force("function param un match !");
        }
    }
}

struct TYPE *read_func_type(char **fname)
{
    struct TYPE *base_ty = read_decl_specifier();
    if(base_ty->storage == KIND_EXTERN) {
        error_force("unexpected storage class");
    }

    struct TYPE *ftype = read_declaretor(base_ty, fname, DECL_DEFAULT);

    struct Node *node = map_get(global_env, *fname);
    if(node) {

        //类型定义不一致
        if(node->kind != AST_FUNC_DEF) {
            error_force("redefine function");
        }
        
        //函数返回值不一致
        if(node->ty->kind != ftype->ret->kind) {
            error_force("function return value un match !");
        }

        //假如两个函数都一致校验是否两个函数的, 参数是否都是变参
        if(node->ty->is_indeterminate != ftype->is_indeterminate) {
            error_force("function param un match !");
        }

        //校验声明的函数与实际定义的函数参数是否一致.
        check_params(node->params, ftype->params);
    }

    return ftype;
}

struct Node *read_func_def()
{
    char *fname = NULL;

    struct TYPE *ftype = read_func_type(&fname);

    //把函数的返回值记录下来, 为了方便在其他的模块直接访问.
    if(ftype->kind != KIND_VOID) {
        func_type = ftype;
    }

    struct Node *body = read_func_body(fname);

    struct Node *func_node = make_func_node(ftype->ret, ftype->params, body);

    map_put(global_env, fname, func_node);

    func_type = NULL;

    return func_node;
}

//is_global 表示当前声明是在全局位置还是在局部位置, TRUE 是全局， FALSE 是局部
struct Node *read_decl(BOOL is_global)
{
    struct Buffer *decl_list = make_buffer();

    struct TYPE *base_ty = read_decl_specifier();
    if(base_ty->kind == 0) {
        //没读到声明
        return NULL;
    }

    struct Node *node = NULL;
    for(;;) {

        char *vname = NULL;
        struct TYPE *ty = read_declaretor(base_ty, &vname, DECL_DEFAULT);

        if(base_ty->storage == KIND_TYPEDEF) {
            make_redefine_node(ty, vname);
            continue;
        }

        //变量名不能出现重复的
        node = map_get(env(), vname);
        if(node) {
            if(ty->kind != KIND_FUNC) {
                error_force("redefine var: %s", vname);
            }
            //重定义函数
            if(ty->kind == KIND_FUNC) {
                error_force("redefine function: %s", vname);
            }
        }

        //这里函数
        if(ty->kind == KIND_FUNC) {

            if(base_ty->storage == KIND_STATIC && is_global) {
                error_force("invalid storage class for function: %s", vname);
            }

            map_put(env(), vname, make_func_node(ty, NULL, NULL));
            expect(',');
            continue;
        } else {

            //需要判断是否是静态 和 环境
            struct Node *node = NULL;

            //不是全局环境, 而且不是外部定义
            if(!is_global && base_ty->storage == KIND_EXTERN) {
                //局部的静态变量
                if(base_ty->storage == KIND_STATIC) {
                    node = make_static_lvar_node(ty, vname);
                } else {
                    //普通局部变量
                    node = make_local_var_node(ty, vname);
                }
            } else {
                //全局变量
                node = make_global_var_node(ty, vname);
            }

            //变量初始化
            if(next_token('=')) {
                node->init_list = read_decl_init(base_ty);
            }

            buffer_push(decl_list, make_decl_node(node));

            if(next_token(',')) {
                continue;
            }
        }

        if(iskeyword(peek_token(), ';')) {
            break;
        }

        error_force("unexpected token: %s", get_token()->token_string);
    }

    return make_compound_node(decl_list);
}

//TODO: 可以把括号存到栈里面校验是否所有的括号都匹配.
void skip_parenthesis(struct Buffer *list)
{
    struct Token *tok = NULL;
    for(;;) {

        tok = get_token();
        if(iskeyword(tok, KIND_EOF)) {
            error_force("un finshed expression!");
        }

        buffer_push(list, tok);

        if(iskeyword(tok, ')')) {
            break;
        }
        if(iskeyword(tok, '(')) {
            skip_parenthesis(list);
        }
    }
}

//判断是否是函数定义，函数定义和声明只能分开处理
//因为函数声明的形参可以不带变量名，但是函数定义的形参必须带名字
//函数定义的特征就是有一个{ 在紧接着) 后面
BOOL is_func_def()
{
    BOOL ret = FALSE;

    struct Token *tok = NULL;
    struct Buffer *list = make_buffer();
    for(;;) {

        tok = get_token();
        if(iskeyword(tok, KIND_EOF)) {
            error_force("un finshed expression!");
        }

        buffer_push(list, tok);

        if(iskeyword(tok, ';')) {
            break;
        }

        if(iskeyword(tok, '(')) {
            skip_parenthesis(list);
            continue;
        }

        if(iskeyword(tok, '{') && istype(peek_token()->kind)) {
            ret = TRUE;
            break;
        }
    }

    //把刚才所有读到的token 放回去, 为了之后的代码分析.
    while(buffer_len(list) > 0) {
        unget_token(buffer_pop(list));
    }

    return ret;
}

struct Buffer *read_source()
{
    struct Buffer *lists = make_buffer();

    struct Token *tok = peek_token();
    for(;;) {

        if(iskeyword(tok, KIND_EOF)) {
            break;
        }

        if(is_func_def()) {
            buffer_push(lists, read_func_def());
        }

        buffer_push(lists, read_decl(TRUE));
    }

    return lists;
}

void init_env()
{
    global_env = make_map();
    local_env = NULL;
}