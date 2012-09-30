#ifndef FXMY_CORE_H
#define FXMY_CORE_H

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

#endif