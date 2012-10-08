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

static void
test_next_token_basic(void)
{
    size_t i;
    const fxmy_char *initial_test[] = {
        NULL,
        C(""),
        C("token")
    };

    const fxmy_char *initial_expected[FXMY_ARRAY_COUNT(initial_test)];
    initial_expected[0] = NULL;
    initial_expected[1] = NULL;
    initial_expected[2] = initial_test[2];

    for (i = 0; i < FXMY_ARRAY_COUNT(initial_test); ++i)
    {
        const fxmy_char *begin;
        const fxmy_char *end;

        begin = fxmy_fnext_token(&end, initial_test[i]);

        TEST(begin == initial_expected[i]);
    }
}

static void
test_tokenize_until_end(void)
{
    size_t i;
    const fxmy_char *test = C("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'");
    const fxmy_char *expected_values[] = { C("SELECT"), C("option_name"), C(","), C("option_value"), C("FROM"), C("wp_options"), C("WHERE"), C("autoload"), C("="), C("'yes'"), NULL };
    const fxmy_char *expected_ptrs[FXMY_ARRAY_COUNT(expected_values)];
    const fxmy_char *begin;
    const fxmy_char *end;

    expected_ptrs[0] = test;
    expected_ptrs[1] = test + 7;
    expected_ptrs[2] = test + 18;
    expected_ptrs[3] = test + 20;
    expected_ptrs[4] = test + 33;
    expected_ptrs[5] = test + 38;
    expected_ptrs[6] = test + 49;
    expected_ptrs[7] = test + 55;
    expected_ptrs[8] = test + 64;
    expected_ptrs[9] = test + 66;
    expected_ptrs[10] = NULL;

    begin = test;

    TEST(FXMY_ARRAY_COUNT(expected_ptrs) == 11);

    for (i = 0; i < FXMY_ARRAY_COUNT(expected_ptrs); ++i)
    {
        size_t length;

        begin = fxmy_fnext_token(&end, begin);
        length = end - begin;

        TEST(begin == expected_ptrs[i]);

        if (begin == NULL)
        {
            TEST(i == (FXMY_ARRAY_COUNT(expected_ptrs) - 1));
        }
        else
        {
            TEST(fxmy_fstrnicmp(expected_values[i], begin, length) == 0);
        }

        begin = end;
    }
}

static const fxmy_char *test_queries[] = {
    C("SELECT option_name, option_value FROM wp_options WHERE autoload = 'yes'"),
    C("SELECT option_name, option_value FROM wp_options"),
    C("SELECT option_value FROM wp_options WHERE option_name = 'siteurl'"),
    C("SELECT foo FROM bar WHERE baz = 'hello how are you\\' today'"),
    C("DESCRIBE wp_users;")
};

struct expected_tokens_t
{
    size_t num_tokens;
    const fxmy_char *string;
    const fxmy_char **tokens;
};

#define INITIALIZE_EXPECTED_TOKENS(x) { \
    expected_tokens[x].num_tokens = FXMY_ARRAY_COUNT(expected ## x); \
    expected_tokens[x].string = test_queries[x]; \
    expected_tokens[x].tokens = expected ## x; \
}

static void
test_next_token(void)
{
    const fxmy_char *expected0[] = { C("SELECT"), C("option_name"), C(","), C("option_value"), C("FROM"), C("wp_options"), C("WHERE"), C("autoload"), C("="), C("'yes'") };
    const fxmy_char *expected1[] = { C("SELECT"), C("option_name"), C(","), C("option_value"), C("FROM"), C("wp_options") };
    const fxmy_char *expected2[] = { C("SELECT"), C("option_value"), C("FROM"), C("wp_options"), C("WHERE"), C("option_name"), C("="), C("'siteurl'") };
    const fxmy_char *expected3[] = { C("SELECT"), C("foo"), C("FROM"), C("bar"), C("WHERE"), C("baz"), C("="), C("'hello how are you\\' today'") };
    const fxmy_char *expected4[] = { C("DESCRIBE"), C("wp_users"), C(";") };

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
        const fxmy_char *begin;
        const fxmy_char *end;

        num_tokens = expected_tokens[i].num_tokens;
        begin = expected_tokens[i].string;

        for (ii = 0; ii < num_tokens; ++ii)
        {
            size_t expected_length = fxmy_fstrlen(expected_tokens[i].tokens[ii]);

            begin = fxmy_fnext_token(&end, begin);
            TEST((size_t)(end - begin) == fxmy_fstrlen(expected_tokens[i].tokens[ii]));
            TEST(fxmy_fstrncmp(begin, expected_tokens[i].tokens[ii], expected_length) == 0);

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
    test_next_token_basic();
    test_tokenize_until_end();
    test_next_token();
    test_rearrange_limit();
}
