/* test/tmpdir.h -- scratch directory for tests that write files */

#ifndef TEST_TMPDIR_H
#define TEST_TMPDIR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* Directory for scratch files, with no trailing separator: "/tmp" on
 * Unix, %TEMP% (or %TMP%) on Windows, "." elsewhere. */
const char *test_tmpdir(void);

/* Write "<test_tmpdir()>/<leaf>" into buf. Returns non-zero on success,
 * zero if it did not fit. */
int test_tmppath(char *buf, size_t cap, const char *leaf);

#ifdef __cplusplus
}
#endif

#endif
