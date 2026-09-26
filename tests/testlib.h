#ifndef TESTLIB_H
# define TESTLIB_H

# include <stdio.h>
# include <stdint.h>

/// Tiny test harness: no framework, just counters and two macros.
///
/// Each test binary is one main() that runs checks and returns
/// test_report(). A failing check prints file:line and the expression and
/// keeps going, so one run shows every failure instead of only the first.

static int	g_checks;
static int	g_failures;

static inline void	test_check(int ok, const char *expr, const char *file,
	int line)
{
	g_checks++;
	if (!ok)
	{
		g_failures++;
		fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, expr);
	}
}

/// Checks a condition. On failure prints the expression text.
# define CHECK(cond) test_check((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

/// Checks two integers are equal. On failure prints the expression text.
# define CHECK_EQ(got, want) \
	test_check((long long)(got) == (long long)(want), \
		#got " == " #want, __FILE__, __LINE__)

/// Prints the summary line and returns the exit code for main().
static inline int	test_report(const char *name)
{
	printf("  %s: %d checks, %d failed\n", name, g_checks, g_failures);
	if (g_failures)
		return (1);
	return (0);
}

/// Deterministic pseudo-random numbers (xorshift32), so a failing random
/// test fails the same way every run. Never seed with 0.
static inline uint32_t	test_rand(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return (*state);
}

#endif
