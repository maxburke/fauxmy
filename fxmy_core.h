#ifndef FXMY_CORE_H
#define FXMY_CORE_H

#ifdef _MSC_VER
#pragma warning(push, 0)
#include <wchar.h>
#pragma warning(pop)
#endif

#define VERIFY_impl(x, line) if (!(x)) { FXMY_PERROR(__FILE__ "(" line "): " #x); abort(); } else (void)0
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define VERIFY(x) VERIFY_impl(x, STRINGIZE(__LINE__))

#ifndef NDEBUG
#define ASSERT(x) VERIFY(x)
#else
#define ASSERT(x)
#endif

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#define FXMY_ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

#define UNUSED(x) (void)x

#define FXMY_OK ((uint64_t)0)
#define FXMY_ERROR ((uint64_t)(int64_t)-1)

enum fxmy_log_level_t
{
    FXMY_LOG_CRITICAL,
    FXMY_LOG_ERROR,
    FXMY_LOG_WARNING,
    FXMY_LOG_INFO,
    FXMY_LOG_ALL        /* This must be the last entry in the enumeration */
};

void
fxmy_set_verbosity_threshold(enum fxmy_log_level_t threshold);

void
fxmy_set_log_file(const char *file);

void
fxmy_log(enum fxmy_log_level_t log_level, const char *format, ...);

void
fxmy_wlog(enum fxmy_log_level_t log_level, const wchar_t *format, ...);

#endif