#include <stdarg.h>
#include <stdio.h>

#include "fxmy_core.h"

static enum fxmy_log_level_t log_threshold;
static FILE *log_file;

void
fxmy_set_verbosity_threshold(enum fxmy_log_level_t threshold)
{
    log_threshold = threshold;
}

void
fxmy_set_log_file(const char *file)
{
    if (log_file != stderr && log_file != stdout)
        fclose(log_file);

    log_file = fopen(file, "w");
}

void
fxmy_log(enum fxmy_log_level_t log_level, const char *format, ...)
{
    va_list args;

    if (log_file == NULL)
        log_file = stderr;

    if (log_level > log_threshold)
        return;

    fprintf(log_file, "[fxmy] ");

    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

void
fxmy_wlog(enum fxmy_log_level_t log_level, const wchar_t *format, ...)
{
    va_list args;

    if (log_file == NULL)
        log_file = stderr;

    if (log_level > log_threshold)
        return;

    fwprintf(log_file, L"[fxmy] ");

    va_start(args, format);
    vfwprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}