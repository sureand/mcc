#include "mcc.h"

struct Buffer *buffs = NULL;

typedef struct {
    int key;
    const char *value;
}KEYWORD;

static KEYWORD keywords[] = {
    #define _KEYWORD(a) KEYWORD_##a

    {_KEYWORD(AUTO), "auto"},
    {_KEYWORD(BREAK), "break"},
    {_KEYWORD(CASE), "case"},
    {_KEYWORD(CHAR), "char"},
    {_KEYWORD(CONST), "const"},
    {_KEYWORD(CONTINUE), "continue"},
    {_KEYWORD(DEFAULT), "default"},
    {_KEYWORD(DO), "do"},
    {_KEYWORD(DOUBLE), "double"},
    {_KEYWORD(ELSE), "else"},
    {_KEYWORD(ENUM), "enum"},
    {_KEYWORD(EXTERN), "extern"},
    {_KEYWORD(FLOAT), "float"},
    {_KEYWORD(FOR), "for"},
    {_KEYWORD(GOTO), "goto"},
    {_KEYWORD(IF), "if"},
    {_KEYWORD(INT), "int"},
    {_KEYWORD(LONG), "long"},
    {_KEYWORD(REGISTER), "register"},
    {_KEYWORD(RETURN), "return"},
    {_KEYWORD(SHORT), "short"},
    {_KEYWORD(SIGNED), "signed"},
    {_KEYWORD(SIZEOF), "sizeof"},
    {_KEYWORD(STATIC), "static"},
    {_KEYWORD(STRUCT), "struct"},
    {_KEYWORD(SWITCH), "switch"},
    {_KEYWORD(TYPEDEF), "typedef"},
    {_KEYWORD(UNION), "union"},
    {_KEYWORD(UNSIGNED), "unsigned"},
    {_KEYWORD(VOID), "void"},
    {_KEYWORD(VOLATILE), "volatile"},
    {_KEYWORD(WHILE), "while"},
    {_KEYWORD(INLINE), "inline"},
    {_KEYWORD(RESTRICT), "restrict"},
    {_KEYWORD(__BOOL), "__Bool"},
    {_KEYWORD(__COMPLEX), "__Complex"},
    {_KEYWORD(__IMAGINARY), "__Imaginary"}

    #undef _KEYWORD
};

BOOL is_keyword(const char *s)
{
    if(!s) {
        return 0;
    }

    size_t i = 0;
    for(; i < sizeof(keywords) / sizeof(KEYWORD); i++)
    {
        if(strcmp(s, keywords[i].value) == 0) {
            return keywords[i].key;
        }
    }

    return 0;
}

void skip_comment() {

    char c1 = read_char();
    if(c1 == '/') {

        char c2 = read_char();

        //skip block /* */
        if(c2 == '*') {
            while(c2 != '*' && c2 != EOF) {
                c2 = read_char();
            }

            if(c2 == EOF) {
                error_force("error, un finished /*\n");
            }

            char c3 = c2;
            while(c3 != '/' && c3 != EOF) {
                c3 = read_char();
            }

            if(c3 == EOF) {
                error_force("error, un finished /*\n");
            }

            unget_char(c3);
            return;
        }

        //skip line //
        if(c2 == '/') {

            while(c2 != '\n' && c2 != EOF) {
                c2 = read_char();
            }

            if(c2 == EOF) {
                error_force("error, un finished /*\n");
            }

            unget_char(c2);
            return;
        }

        unget_char(c2);
        unget_char(c1);

        return;
    }
}

struct Token *make_token(int kind, const char *s)
{
    struct Token *tok = (struct Token *)calloc(1, sizeof(struct  Token));

    tok->kind = kind;
    tok->file = get_current_file();
    tok->token_string = s;

    return tok;
}

struct Token *make_space_tok(const char *s)
{
    return make_token(KIND_SPACE, s);
}

struct Token *read_white_space()
{
    char c = read_char();
    if(c > 32) {
        error_force("error, un expect char!\n");
    }
    
    struct String *str = make_String();

    while(c <= 32) {
        String_append(str, c);
        c = read_char();
    }

    unget_char(c);

    return make_space_tok(String_move(str));
}

struct Token *make_float_number(const char *s)
{
    return make_token(KIND_FLOAT, s);
}

struct Token *make_double_number(const char *s)
{
    return make_token(KIND_DOUBLE, s);
}

struct Token *make_long_number(const char *s)
{
    return make_token(KIND_LONG, s);
}

struct Token *make_number(int kind, int id, const char *s)
{
    struct Token *tok = make_token(kind, s);
    tok->id = id;

    return tok;
}

static BOOL is_hex(char c)
{
    c = tolower(c);
    return c >= 'a' && c <= 'f';
}

static BOOL is_octal(const char *number)
{
    if(!number) {
        return FALSE;
    }

    const char *p = number;
    while(*p && *p >= '0' && *p <= '7') ++p;

    return !p;
}

struct Token *read_number_tok()
{
    char c1 = read_char();

    if(!isdigit(c1)){
        error_force("unknow number!\n");
    }

    struct String *str = make_String();

    if(c1 == '0') {
        //16 band
        char c2 = read_char();
        if(c2 == 'x' || c2 == 'X') {
            String_append(str, c1);
            String_append(str, c2);

            char c3 = read_char();
            while(isalpha(c3) || is_hex(c3)) {
                String_append(str, c3);
                c3 = read_char();
            }

            unget_char(c3);
            return make_number(KIND_NUMBER, KIND_HEX, String_move(str));
        }
    }

    String_append(str, c1);

    c1 = read_char();
    while(isdigit(c1)) {
        String_append(str, c1);
        c1 = read_char();
    }

    if(c1 != '.') {
        const char *number = String_move(str);
        int kind = KIND_INTEGER;
        if(is_octal(number)) {
            kind = KIND_OCTAL;
        }
        return make_number(KIND_NUMBER, kind, String_move(str));
    }

    char c2 = read_char();
    while(isdigit(c2)) {
        String_append(str, c2);
        c2 = read_char();
    }

    if(c2 == 'f' || c2 == 'F') {
        String_append(str, c2);
        return make_number(KIND_NUMBER, KIND_FLOAT, String_move(str));
    }

    unget_char(c1);

    return make_number(KIND_NUMBER, KIND_DOUBLE, String_move(str));
}

struct Token *make_ident_tok(const char *s)
{
    return make_token(KIND_IDENT, s);
}

struct Token *read_ident_tok()
{
    char c1 = read_char();

    if(!(isalpha(c1) && c1 == '_')) {
        error_force("un expect ident!\n");
    }

    struct String *str = make_String();

    char c2 = read_char();
    while(isalpha(c2) || isdigit(c2) || c2 == '_') {
        String_append(str, c2);
        c2 = read_char();
    }

    unget_char(c2);

    return make_ident_tok(String_dup(str));
}

struct Token *make_keyword(int id, const char *s)
{
    struct Token *tok = make_token(KIND_KEYWORD, s);
    tok->id = id;

    return tok;
}

BOOL is_trans(char c)
{
    switch (c)
    {
        case 'a':
        case 'b':
        case 'r':
        case 'v':
        case 'n':
        case 't':
        case 'f':
        case '\'':
        case '\"':
        case '\\':
        case '0':
            return TRUE;
        default:
            break;
    }

    return FALSE;
}

static char trans(char c)
{
    switch (c)
    {
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 'r':
            return '\r';
        case 'v':
            return '\v';
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'f':
            return '\f';
        case '\'':
            return '\'';
        case '\"':
            return '\"';
        case '\\':
            return '\\';
        case '0':
            return '\0';
    }

    return c;
}

struct Token *make_string_tok(const char *s)
{
   return make_token(KIND_STRING, s);
}

struct Token *read_string_tok()
{
    expect('\"');

    struct String *str = make_String();

    char c1 = read_char();
    while(isalpha(c1)) {
        if(c1 == '\\') {
            char c2 = read_char();
            if(is_trans(c2)) {

                char c3 = trans(c2);
                String_append(str, c3);

                continue;
            }

            String_append(str, c1);
            String_append(str, c2);
            continue;
        }

        if(c1 == '%') {
            char c2 = read_char();
            if(c2 == '%') {
                String_append(str, '%');
                continue;
            }
            unget_char(c2);
        }
        String_append(str, c1);
    }

    unget_char(c1);
    expect('\"');

    char *s = String_move(str);

    return make_string_tok(s);
}

static struct Token *read_char_tok()
{
    expect('\'');
    struct String *str = make_String();

    char c1 = read_char();
    if(c1 == '\\'){
        char c2 = read_char();
        if(!is_trans(c2)) {
            error_force("error un expect trans\n");
        }

        char c3 = trans(c2);
        String_append(str, c3);
    }

    expect('\'');

    return make_token(KIND_CHAR, String_move(str));
}

static struct Token *read_keyword_or_ident_tok()
{
    struct Token *tok = read_ident_tok();

    int id = is_keyword(tok->token_string);
    if(!id) {
        return tok;
    }

    tok->kind = KIND_KEYWORD;
    tok->id = id;

    return tok;
}

struct Token *read_token()
{
    skip_comment();

    char c = peek_char();

    if(isdigit(c)){
        return read_number_tok();
    }

    if(isalpha(c) || c == '_') {
        return read_keyword_or_ident_tok();
    }

    if(c == '\'') {
       return read_char_tok();
    }
            
    if(c == '\"') {
        return read_string_tok();
    }

    struct String *str = make_String();

    switch (c)
    {
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case ':':
        case '?':
        case ',':
        case ';':
        case '#':
        {
            String_append(str, c);
            return make_keyword(c, String_move(str));
        }
        case '=':
            return next_char('=') ? make_keyword(OP_EQU, "==") : make_keyword('=', "=");
        case '*':
            return next_char('=') ? make_keyword(OP_MUL_EQU, "*=") : make_keyword('*', "*");
        case '/':
            return next_char('=') ? make_keyword(OP_SUB_EQU, "/=") : make_keyword('/', "/");
        case '%':
            return next_char('=') ? make_keyword(OP_MOD_EQU, "%%=") : make_keyword('%', "%%");
        case '~':
            return next_char('=') ? make_keyword(OP_NEG_EQU, "~=") : make_keyword('~', "~");
        case '!':
            return next_char('=') ? make_keyword(OP_NOT_EQU, "!=") : make_keyword('!', "!");
        case EOF:
            return make_keyword(KIND_EOF, "eof");

        case '.':
        {
            char c2 = read_char();
            if(c2 != '.') {
                unget_char(c2);
                return make_keyword('.', ".");
            }

            char c3 = read_char();
            if(c3 == '.') {
                return make_keyword(KEYWORD_ELLIPSIS, "...");
            }

            error_force("incomplete ellipsis!");
        }

        case '+':
        {
            char c2 = read_char();
            if(c2 == '+') {
                return make_keyword(OP_INC, "++");
            }
            if(c2 == '=') {
                return make_keyword(OP_ADD_EQU, "+=");
            }

            unget_char(c2);
            return make_keyword('+', "+");
        }
        case '-':
        {
            char c2 = read_char();
            if(c2 == '-') {
                return make_keyword(OP_DEC, "--");
            }

            if(c2 == '=') {
                return make_keyword(OP_SUB_EQU, "-=");
            }

            if(c2 == '>') {
                return make_keyword(OP_ARROW, "->");
            }

            unget_char(c2);
            return make_keyword('-', "-");
        }
        case '&':
        {
            char c2 = read_char();
            if(c2 == '&') {
                return make_keyword(OP_LOG_AND, "&&");
            }

            if(c2 == '=') {
                return make_keyword(OP_BIT_AND_EQU, "&=");
            }

            unget_char(c2);
            return make_keyword('&', "&");
        }
        case '|':
        {
            char c2 = read_char();
            if(c2 == '|') {
                return make_keyword(OP_LOG_OR, "||");
            }

            if(c2 == '=') {
                return make_keyword(OP_BIT_OR_EQU, "|=");
            }

            unget_char(c2);
            return make_keyword('|', "|");
        }
        case '>':
        {
            char c2 = read_char();
            if(c2 == '=') {
                return make_keyword(OP_BIG_QUE, ">=");
            }

            if(c2 == '>') {
                return make_keyword(OP_SAR, ">>");
            }

            unget_char(c2);
            return make_keyword('>', ">");
        }

        case '<':
        {
            char c2 = read_char();
            if(c2 == '=') {
                return make_keyword(OP_LITTLE_EQU, "<=");
            }

            if(c2 == '<') {
                return make_keyword(OP_SAL, "<<");
            }

            unget_char(c2);
            return make_keyword('<', "<");
        }

        default:
            break;
    }

    error_force("error unkown char");

    return NULL;
}

struct Token *get_token()
{
    if(buffer_len(buffs)) {
        return buffer_pop(buffs);
    }

    struct Token *tok = read_token();
    while(tok->kind == KIND_SPACE) {
        tok = read_token();
    }

    return tok;
}

BOOL next_token(int id)
{
    struct Token *tok = get_token();

    if(tok->id == id) {
        return TRUE;
    }

    unget_token(tok);

    return FALSE;
};

struct Token *peek_token()
{
    struct Token *tok = get_token();

    unget_token(tok);

    return tok;
}

void unget_token(struct Token *tok)
{
    buffer_push(buffs, tok);
}

BOOL expect(int c) 
{
    struct Token *tok = get_token();

    if(tok->id == c) {
        return TRUE;
    }

    error_force("expected ‘%c’ before ‘%s’ token ", c, tok->token_string);

    return FALSE;
}

struct Buffer *init_buffer()
{
    buffs = make_buffer();

    return buffs;
}

void lex_init()
{
    init_file();
    init_buffer();
}