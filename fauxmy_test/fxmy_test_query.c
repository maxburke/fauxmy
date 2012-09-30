#include <string.h>

#include "fxmy_core.h"
#include "fxmy_query.h"
#include "fxmy_test.h"

static const char *test_queries[] = {
    "SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'",
    "SELECT option_name, option_value FROM wp_options",
    "SELECT option_value FROM wp_options WHERE option_name = 'siteurl'",
    "DESCRIBE wp_users;"
};

struct expected_tokens_t
{
    size_t num_tokens;
    const char *string;
    const char **tokens;
};

#define INITIALIZE_EXPECTED_TOKENS(x) { \
    expected_tokens[x].num_tokens = FXMY_ARRAY_COUNT(expected ## x); \
    expected_tokens[x].string = test_queries[x]; \
    expected_tokens[x].tokens = expected ## x; \
}

static void
test_next_token(void)
{
    const char *expected0[] = { "SELECT", "option_name", ",", "option_value", "FROM", "wp_options", "WHERE", "autoload", "=", "'yes'" };
    const char *expected1[] = { "SELECT", "option_name", ",", "option_value", "FROM", "wp_options" };
    const char *expected2[] = { "SELECT", "option_value", "FROM", "wp_options", "WHERE", "option_name", "=", "'siteurl'" };
    const char *expected3[] = { "DESCRIBE", "wp_users", ";" };

    struct expected_tokens_t expected_tokens[FXMY_ARRAY_COUNT(test_queries)];

    size_t i;
    size_t ii;

    INITIALIZE_EXPECTED_TOKENS(0);
    INITIALIZE_EXPECTED_TOKENS(1);
    INITIALIZE_EXPECTED_TOKENS(2);
    INITIALIZE_EXPECTED_TOKENS(3);
    
    for (i = 0; i < FXMY_ARRAY_COUNT(expected_tokens); ++i)
    {
        size_t num_tokens;
        const char *begin;
        const char *end;

        num_tokens = expected_tokens[i].num_tokens;
        begin = expected_tokens[i].string;

        for (ii = 0; ii < num_tokens; ++ii)
        {
            begin = fxmy_next_token(&end, begin);
            TEST(strncmp(begin, expected_tokens[i].tokens[ii], strlen(expected_tokens[i].tokens[ii])) == 0);

            begin = end;
        }
    }
}

void
test_query(void)
{
    test_next_token();
}