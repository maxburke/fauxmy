#include <string.h>
#include <stdlib.h>

#include "fxmy_core.h"
#include "fxmy_mem.h"
#include "fxmy_query.h"
#include "fxmy_string.h"
#include "fxmy_test.h"

#ifndef MAX
#define MAX(a, b) ((a)>(b)?(a):(b))
#endif

static const char *test_queries[] = {
    "SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'",
    "SELECT option_name, option_value FROM wp_options",
    "SELECT option_value FROM wp_options WHERE option_name = 'siteurl'",
    "SELECT foo FROM bar WHERE baz = 'hello how are you\\' today'",
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
    const char *expected3[] = { "SELECT", "foo", "FROM", "bar", "WHERE", "baz", "=", "'hello how are you\\' today'" };
    const char *expected4[] = { "DESCRIBE", "wp_users", ";" };

    struct expected_tokens_t expected_tokens[FXMY_ARRAY_COUNT(test_queries)];

    size_t i;
    size_t ii;

    INITIALIZE_EXPECTED_TOKENS(0);
    INITIALIZE_EXPECTED_TOKENS(1);
    INITIALIZE_EXPECTED_TOKENS(2);
    INITIALIZE_EXPECTED_TOKENS(3);
    INITIALIZE_EXPECTED_TOKENS(4);
    
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
            TEST((size_t)(end - begin) == strlen(expected_tokens[i].tokens[ii]));
            TEST(strncmp(begin, expected_tokens[i].tokens[ii], strlen(expected_tokens[i].tokens[ii])) == 0);

            begin = end;
        }
    }
}

static const fxmy_char *limit_test_strings[] = {
    C("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes' LIMIT 100"),
    C("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes' LIMIT 100;"),
};

static const fxmy_char *limit_expected_strings[] = {
    C("SELECT TOP  100 option_name, option_value FROM wp_options WHERE autoload = 'yes' "),
    C("SELECT TOP  100 option_name, option_value FROM wp_options WHERE autoload = 'yes' ;"),
};

static void
test_rearrange_limit(void)
{
    size_t i;

    for (i = 0; i < FXMY_ARRAY_COUNT(limit_test_strings); ++i)
    {
        const fxmy_char *test_string;
        const fxmy_char *expected_string;
        fxmy_char *test_buffer;
        size_t test_string_length;
        size_t test_string_length_incl_null;
        
        test_string = limit_test_strings[i];
        expected_string = limit_expected_strings[i];
        test_string_length = fxmy_fstrlen(test_string);
        test_string_length_incl_null = test_string_length + 1;

        TEST(test_string_length == fxmy_fstrlen(limit_expected_strings[i]));

        test_buffer = fxmy_calloc(test_string_length_incl_null, sizeof(fxmy_char));
        memcpy(test_buffer, test_string, test_string_length * sizeof(fxmy_char));

        TEST(!fxmy_rearrange_limit(test_buffer));
        TEST(memcmp(test_buffer, expected_string, test_string_length_incl_null) == 0);

        fxmy_free(test_buffer);
    }
}

void
test_query(void)
{
    test_next_token();
    test_rearrange_limit();
}
