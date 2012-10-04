#include <stdio.h>
#include <stdarg.h>

#include "fxmy_core.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#pragma warning(push, 0)
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(pop)
#endif

static int fxmy_test_will_break_on_fail;
static int fxmy_test_verbose;
static int fxmy_num_tests_run;
static int fxmy_num_tests_failed;

void
fxmy_test_set_break_on_fail(int break_on_fail)
{
    fxmy_test_will_break_on_fail = break_on_fail;
}

int
fxmy_test_break_on_fail(void)
{
    return fxmy_test_will_break_on_fail;
}

void
fxmy_test_set_verbose(int verbose)
{
    fxmy_test_verbose = verbose;
}

static
void fxmy_test_print(const char *format, ...)
{
    static char buf[1024];
    va_list v;

    va_start(v, format);

    vsnprintf(buf, FXMY_ARRAY_COUNT(buf), format, v);
    fputs(buf, stderr);

#ifdef _MSC_VER
    OutputDebugString(buf);
#endif

    va_end(v);
}

int
fxmy_test_report_fail(const char *file, int line, const char *message)
{
    ++fxmy_num_tests_failed;

    fxmy_test_print("%s(%d): FAIL: %s\n", file, line, message);

    return 0;
}

int
fxmy_test_report_pass(const char *file, int line, const char *message)
{
    if (fxmy_test_verbose)
        fxmy_test_print("%s(%d): PASS: %s\n", file, line, message);

    return 1;
}

int
fxmy_test_report(int result, const char *file, int line, const char *message)
{
    ++fxmy_num_tests_run;

    return result ? fxmy_test_report_pass(file, line, message) : fxmy_test_report_fail(file, line, message);
}

void
test_query(void);

void
test_string(void);

int
main(void)
{
    if (IsDebuggerPresent())
        fxmy_test_set_break_on_fail(1);

    test_query();
    test_string();

    fxmy_test_print("%d / %d failed\n", fxmy_num_tests_failed, fxmy_num_tests_run);

    return fxmy_num_tests_failed;
}