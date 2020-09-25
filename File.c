#include "mcc.h"

struct File *current_file = NULL;
struct Buffer *file_list = NULL;

struct POS *current_pos = NULL;

struct Buffer *files_buf = NULL;

struct File *get_current_file()
{
    return current_file;
}

struct Buffer *get_files()
{
    return file_list;
}

static struct POS *make_pos()
{
    struct POS *pos = (struct POS*)calloc(1, sizeof(struct POS));

    return pos;
}

char get_char()
{
    if(current_file == NULL) {
        return EOF;
    }

    char c = *current_file->src++;
    if(c == '\n') {
        ++current_file->line;
        current_file->column = 1;
    }
    else {
        ++current_file->column;
    }

    if(c == EOF) {

        //获取缓存的文件.
        if(buffer_len(files_buf) > 0) {
            current_file = buffer_pop(files_buf);
            buffer_push(file_list, current_file);
            return get_char();
        }

        //到达文件开头
        if(buffer_len(file_list) <= 1) {
            return c;
        }
    }
    
    return c;
}

void unget_char(char c)
{
    if(current_file->line == 1 && current_file->column == 1) {
   
        //队列只有一个文件，回到最开始了.
        if(buffer_len(file_list) <= 1) {
            return;
        }

        //回退到上一个文件
        buffer_push(files_buf, current_file);

        current_file = buffer_pop(file_list);
    }

    --current_file->src;
    if(c == '\n') {
        --current_file->line;
        current_file->column = 1;
    }
    else {
        --current_file->column;
    }
}

char read_char()
{
    for(;;)
    {
        char c1 = get_char();
        if(c1 == '\\') {
            char c2 = get_char();
            if(c2 == '\n') {
                continue;
            }
            unget_char(c2);
        }

        return c1;
    }
}

BOOL next_char(char c)
{
    if(read_char() == c) {
        return TRUE;
    }

    unget_char(c);

    return FALSE;
}

char peek_char()
{
    char c = read_char();
    unget_char(c);

    return c;
}

struct POS *get_pos()
{
    current_pos->column = current_file->column;
    current_pos->line = current_file->line;
    current_pos->filename = current_file->filename;

    return current_pos;
}

static const char *read_file(const char *filename)
{
    if(filename == NULL) {
        return NULL;
    }

    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        return NULL;
    }

    long len = ftell(fp);
    
    if(len > 0) {

        char *buffer = (char*)malloc(sizeof(char));
        if(buffer == NULL) {
            return NULL;
        }

        size_t pos = 0;
        while(pos = fread(buffer + pos, sizeof(char), sizeof(len), fp));

        fclose(fp);

        return buffer;
    }

    return NULL;
}

static struct File *do_make_file(const char *filename, const char *src)
{
    struct File *file = (struct File*)calloc(1, sizeof(struct File));

    file->filename = filename;
    file->line = 1;
    file->column = 1;

    file->src = filename ? read_file(filename) : src;

    return file;
}

struct File *make_file(const char *filename)
{
    return do_make_file(filename, NULL);
}

struct File *make_string_file(const char *src)
{
    return do_make_file(NULL, src);
}

void add_file(struct File *file)
{
    buffer_push(file_list, file);
}

void init_pos()
{
    if(!current_pos) {
        current_pos = make_pos();
    }
}
void init_file()
{
    files_buf = make_buffer();

    if(file_list) {
        current_file = buffer_tail(file_list);
    }
}