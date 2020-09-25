#ifndef __MCC_HEADER__
#define __MCC_HEADER__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef BOOL
#define BOOL unsigned char
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL 
#define NULL ((void*)0)
#endif

//kind 
enum {

    //kind
    KIND_IDENT = 0x0A,
    KIND_KEYWORD,
    KIND_ARRAY,
    KIND_ENUM,
    KIND_FUNC,
    KIND_STRUCT,
    KIND_UNION,

    KIND_REGISTER,
    KIND_STATIC,
    KIND_EXTERN,
    KIND_TYPEDEF,

    //type
    KIND_AUTO,
    KIND_NUMBER,
    KIND_HEX,
    KIND_OCTAL,
    KIND_VOID,
    KIND_INTEGER,
    KIND_FLOAT,
    KIND_LONG,
    KIND_DOUBLE,
    KIND_SHORT,
    KIND_UNSIGNED_SHORT,
    KIND_SIGNED,
    KIND_UNSIGNED,
    KIND_UNSIGNED_INT,
    KIND_STRING,
    KIND_UNSIGNED_CHAR,
    KIND_CHAR,
    KIND_POINTER,
    KIND_SPACE,
    KIND_EOF,

    //temp kind
    KIND_TEMP
};

//keyword
enum
{
    #define _KEYWORD(a) KEYWORD_##a

    _KEYWORD(UNKOWN) = 0x110, //0xA + FF
    _KEYWORD(AUTO),  _KEYWORD(BREAK), _KEYWORD(CASE),
    _KEYWORD(CHAR),  _KEYWORD(CONST), _KEYWORD(CONTINUE),
    _KEYWORD(DEFAULT), _KEYWORD(DO), _KEYWORD(DOUBLE),
    _KEYWORD(ELSE), _KEYWORD(ENUM), _KEYWORD(EXTERN),
    _KEYWORD(FLOAT), _KEYWORD(FOR), _KEYWORD(GOTO),
    _KEYWORD(IF), _KEYWORD(INT), _KEYWORD(LONG),
    _KEYWORD(REGISTER), _KEYWORD(RETURN), _KEYWORD(SHORT),
    _KEYWORD(SIGNED), _KEYWORD(SIZEOF), _KEYWORD(STATIC),
    _KEYWORD(STRUCT), _KEYWORD(SWITCH), _KEYWORD(TYPEDEF),
    _KEYWORD(UNION), _KEYWORD(UNSIGNED), _KEYWORD(VOID),
    _KEYWORD(VOLATILE), _KEYWORD(WHILE), _KEYWORD(INLINE),
    _KEYWORD(__BOOL), _KEYWORD(__COMPLEX), _KEYWORD(__IMAGINARY),
    _KEYWORD(ELLIPSIS),
    _KEYWORD(RESTRICT)

    #undef _KEYWORD
};

//op
enum {
    OP_INC = 0x20F, // 0x110 + 0xFF,  ++
    OP_DEC, // --
    OP_PRE_INC,
    OP_PRE_DEC,
    OP_MUL_EQU, // *=
    OP_DIV_EQU, // /=
    OP_MOD_EQU, // %=
    OP_EQU, // ==
    OP_BIG_QUE, // >=
    OP_LITTLE_EQU, // <=
    OP_ADD_EQU, // +=
    OP_SUB_EQU, // -=
    OP_SIZE_OF, //sizeof
    OP_ARROW, // ->
    OP_LOG_AND, // &&
    OP_LOG_OR, // ||
    OP_LOG_NOT, // !
    OP_QUESTION, // ?
    OP_NOT_EQU, // !=
    OP_BIT_OR_EQU, // |=
    OP_BIT_AND_EQU, // &=
    OP_XOR_EQU, // ^=
    OP_SAR, // >>
    OP_SAL, // <<
    OP_NEG, //~
    OP_NEG_EQU // ~=

};

//AST
enum {
    AST_ADDR,
    AST_LOCAL_VAR,
    AST_GLOBAL_VAR,
    AST_LITERAL,
    AST_DECL,
    AST_DEREF,
    AST_COMPOUND,
    AST_CAST,
    AST_CONV,
    AST_IF,
    AST_STRUCT_REF,
    AST_UNARY,
    AST_BINARY,
    AST_WHILE,
    AST_DO_WHILE,
    AST_FUNC_DEF,
    AST_FUNC_CALL,
    AST_RETURN,
    AST_LABEL,
    AST_GOTO,
    AST_TYPEDEF
};

//declare state
//普通的变量声明, 函数声明
//强转
//函数参数声明

enum {
    DECL_DEFAULT,
    DECL_CAST,
    DECL_PARAM,
    DECL_FUNC
};

struct String
{
    size_t len;
    char *data;
    size_t capacity;
};

struct Buffer
{
    size_t len;
    size_t pos;
    void **body;
    size_t capacity;
};

struct Map {
    struct Map *parent;
    char **key;
    void **val;
    int size;
    int nelem;
    int nused;
};

struct File
{
    const char *filename;
    size_t line;
    size_t column;
    const char *src;
};

struct Token
{
    int kind;
    int id;
    struct File *file;

    const char *token_string;
};

struct Dict {
    struct Map *map;
    struct Vector *key;
};

struct TYPE
{
    int kind;
    int len;
    
    size_t align;

    BOOL is_unsigned;

    //struct
    struct Dict *fields;
    char *type_name;
    size_t offset;
    size_t bitsize;
    size_t bitoffset;

    //func
    struct Buffer *params;
    struct TYPE *ret;
    BOOL is_indeterminate;

    struct TYPE *ptr;

    int storage;
    int kind_qualiter;
    int size;
};

struct POS
{
    const char *filename;
    size_t line;
    size_t column;
};

struct Node {

    int kind;
    int id;

    struct TYPE *ty;
    union 
    {
        //integer
        struct {
            long ival;
        };

        //double
        struct {
           double fval;
        };

        //variable
        struct {
            struct Node *varnode;
            const char *varname;
            struct Buffer *init_list;
        };

        //binary
        struct {
           struct Node *left;
           struct Node *right;
        };

        //string
        struct {
            size_t slen;
            char *strval;
        };
        
        //stmt
        struct 
        {
            struct Buffer *lists;
        };

        //function
        struct {
            const char *fname;
            struct Buffer *params;
            struct Buffer *args;
            struct Node *body;
            struct Buffer *vars;
        };

        //struct or union
        struct {
            struct Node *struc;
            struct Map *fields;
        };

        //struct ref
        struct 
        {
            char *field;
        };

        //typedef
        struct 
        {
            char *def_name;
        };

        //label or case or goto
        struct 
        {
            int pos;
            const char *label;
        };

        //if statement or other condition statement
        struct {
            struct Node *condition;
            struct Node *then;
            struct Node *els;
        };

        //unary
        struct {
            struct Node *operand;
        };
    };
};

//Node 
struct TYPE *make_type();
struct Node *make_node();

//File
char get_char();
void unget_char(char c);
char read_char();
char peek_char();
BOOL next_char(char c);
struct File *make_file(const char *filename);
struct File *make_string_file(const char *src);
void add_file(struct File *file);
void init_file();
struct File *get_current_file();
struct Buffer *get_files();
struct POS *get_pos();

//lex
struct Token *peek_token();
struct Token *read_token();
BOOL next_token(int id);
struct Token *get_token();
void unget_token(struct Token *tok);
BOOL test_tok(struct Token *tok, int c);
BOOL expect(int c);

//String
struct String *make_String();
char *String_add(struct String *str, char *s);
char *String_append(struct String *str, char c);
char *String_dup(struct String *str);
char *String_move(struct String *str);
size_t String_len(struct String *str);

//buffer
struct Buffer *make_buffer();
void buffer_push(struct Buffer *buf, void *elem);
void *buffer_pop(struct Buffer *buf);
void *buffer_get(struct Buffer *buf, int index);
void buffer_set(struct Buffer *buf, int index, void *elem);
void *buffer_head(struct Buffer *buf);
void *buffer_tail(struct Buffer *buf);
void *buffer_dup(struct Buffer *buf, void *dst, size_t size);
size_t buffer_len(struct Buffer *buf);

//parse
struct Node *read_expr();
struct Node *read_stmt_expr();
struct Node *read_cast_expr();
struct TYPE *read_decl_specifier();
struct Node *read_decl_stmt();
struct TYPE *read_declaretor(struct TYPE *base_ty, char **ident_name, int state);
struct Node *read_decl(BOOL is_global);
struct Node *read_for_expr();
int read_interpreter_expr();
struct Node *read_primary_exp();
struct Node *read_mul_expr();
struct Node *read_add_expr();
struct Node *read_condition_expr();
struct Node *read_assignment_expr();
struct Node *read_goto_expr();
struct Node *read_boolean_expr();
struct Node *read_compound_expr();
struct Node *read_continue_expr();
struct Node *read_if_expr();
struct Node *read_while_expr();
struct Node *read_case_expr();
struct Node *read_do_while_expr();
struct Node *read_for_expr();
struct Node *read_break_expr();
struct TYPE *read_declaretor_tail(struct TYPE *base_ty, int state);
struct TYPE *read_enum_fields(struct TYPE **type_ptr);
struct Buffer *read_decl_init(struct TYPE *ty);

//error
void error_warn(const char *fmt, ...);
void error_force(const char *fmt, ...);

//map
struct Map *make_map();
void *map_get(struct Map *m, char *key);
void map_put(struct Map *m, char *key, void *val);
struct Map *make_map_parent(struct Map *parent);

//dict
struct Dict *make_dict();
void dict_put(struct Dict *dict, char *key, void *val);
void *dict_get(struct Dict *dict, char *key);
struct Buffer *dict_keys(struct Dict *dict);

#endif