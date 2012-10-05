#ifndef FXMY_TEST_H
#define FXMY_TEST_H

void
fxmy_test_set_break_on_fail(int break_on_fail);

int
fxmy_test_break_on_fail(void);

void
fxmy_test_set_verbose(int verbose);

int
fxmy_test_report(int result, const char *file, int line, const char *message);

#define TEST(x) if (!fxmy_test_report(x, __FILE__, __LINE__, #x) && fxmy_test_break_on_fail()) { __debugbreak(); }

#endif