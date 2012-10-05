#include "fxmy_core.h"
#include "fxmy_string.h"
#include "fxmy_test.h"

#ifdef _MSC_VER
#pragma warning(disable:4204)
#endif

struct fxmy_expected_char_t
{
    const char *needle;
    const char *haystack;
    const char *result;
};

struct fxmy_expected_fxmy_char_t
{
    const fxmy_char *needle;
    const fxmy_char *haystack;
    const fxmy_char *result;
};

void
test_string(void)
{
    size_t i;

    const char * const narrow_test_haystacks[] = {
        "SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'",
        "DESCRIBE wp_users;"
    };

    const struct fxmy_expected_char_t narrow_tests[] =
    {
        { "SELECT", narrow_test_haystacks[0], narrow_test_haystacks[0] },
        { "sELECT", narrow_test_haystacks[0], narrow_test_haystacks[0] },
        { "select", narrow_test_haystacks[0], narrow_test_haystacks[0] },
        { "SELECt", narrow_test_haystacks[0], narrow_test_haystacks[0] },
        { ",", narrow_test_haystacks[0], narrow_test_haystacks[0] + 18 },
        { "option_value", narrow_test_haystacks[0], narrow_test_haystacks[0] + 20 },
        { "OPTION_VALUE", narrow_test_haystacks[0], narrow_test_haystacks[0] + 20 },
        { "OPTION_value", narrow_test_haystacks[0], narrow_test_haystacks[0] + 20 },
        { "OPTION-VALUE", narrow_test_haystacks[0], NULL },
        { "'yes'", narrow_test_haystacks[0], narrow_test_haystacks[0] + 66 },
        { ";", narrow_test_haystacks[1], narrow_test_haystacks[1] + 17 },
        { "does not exist", narrow_test_haystacks[0], NULL }
    };

    const fxmy_char *wide_test_haystacks[] = {
        C("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
        C("DESCRIBE wp_users;")
    };

    const struct fxmy_expected_fxmy_char_t wide_tests[] =
    {
        { C("SELECT"), wide_test_haystacks[0], wide_test_haystacks[0] },
        { C("sELECT"), wide_test_haystacks[0], wide_test_haystacks[0] },
        { C("select"), wide_test_haystacks[0], wide_test_haystacks[0] },
        { C("SELECt"), wide_test_haystacks[0], wide_test_haystacks[0] },
        { C(","), wide_test_haystacks[0], wide_test_haystacks[0] + 18 },
        { C("option_value"), wide_test_haystacks[0], wide_test_haystacks[0] + 20 },
        { C("OPTION_VALUE"), wide_test_haystacks[0], wide_test_haystacks[0] + 20 },
        { C("OPTION_value"), wide_test_haystacks[0], wide_test_haystacks[0] + 20 },
        { C("OPTION-VALUE"), wide_test_haystacks[0], NULL },
        { C("'yes'"), wide_test_haystacks[0], wide_test_haystacks[0] + 66 },
        { C(";"), wide_test_haystacks[1], wide_test_haystacks[1] + 17 },
        { C("does not exist"), wide_test_haystacks[0], NULL }
    };

    for (i = 0; i < FXMY_ARRAY_COUNT(narrow_tests); ++i)
        TEST(fxmy_stristr(narrow_tests[i].haystack, narrow_tests[i].needle) == narrow_tests[i].result);

    for (i = 0; i < FXMY_ARRAY_COUNT(narrow_tests); ++i)
        TEST(fxmy_fstristr(wide_tests[i].haystack, wide_tests[i].needle) == wide_tests[i].result);
}