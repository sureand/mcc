#include "mcc.h"

static void error_t(const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(1);
}

static void warn_t(const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

static void error_level(const char *level, const char *fmt, va_list args)
{
    struct POS *pos = get_pos();

    fprintf(stderr, "%s:%ld:%ld %s: ", pos->filename, pos->line, pos->column, level);

    error_t(fmt, args);
}

static warn_level(const char *level, const char *fmt, va_list args)
{
    struct POS *pos = get_pos();

    fprintf(stderr, "%s:%ld:%ld %s: ", pos->filename, pos->line, pos->column, level);

    warn_t(fmt, args);
}

void error_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    warn_level("warn", fmt, args);
    va_end(args);
}

void error_force(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    error_level("error", fmt, args);
    va_end(args);
}
