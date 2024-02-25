/// Very simple test setup for LinuxCNC-Ethercat.
///
/// To define tests, create a function called `int test_foo(void)`.
/// In the first line, call `TESTSETUP;`.  In the last line, call
/// `TESTRESULTS;`.  In between, do whatever setup is needed and call
/// the `TEST()` macro to actually run tests.  It should be called
/// like `TEST(int, lcec_lookupint(foo), 10, "Wanted %d, got %d");`
/// The first arg is the result type, the next arg is the actual call
/// that you're testing, and the third (and following) args are a
/// printf format string (plus optional args) to print when the test
/// fails.  There will be two additional args passed to printf, the
/// result, and what was expected.  I'm generally following the way
/// that Go does tests here, with `got=%d, want=%d` as the expected
/// pattern for test failures.

#define TESTSETUP int pass = 0, fail = 0

#define TESTINT(call, want)                                                                          \
  do {                                                                                               \
    int got;                                                                                         \
    if (got = call, got != want) {                                                                   \
      fprintf(stderr, "fail at %s:%d: %s, got %d, want %d\n", __FILE__, __LINE__, #call, got, want); \
      fail++;                                                                                        \
    } else {                                                                                         \
      pass++;                                                                                        \
    }                                                                                                \
  } while (0)

#define TESTSTRING(call, want)                                                                          \
  do {                                                                                               \
    const char *got;                                                                                         \
    if (got = call, strcmp(got, want)) {				\
      fprintf(stderr, "fail at %s:%d: %s, got \"%s\", want \"%s\"\n", __FILE__, __LINE__, #call, got, want); \
      fail++;                                                                                        \
    } else {                                                                                         \
      pass++;                                                                                        \
    }                                                                                                \
  } while (0)

#define TESTNOTNULL(call)                                                                   \
  do {                                                                                      \
    void *got;                                                                              \
    if (got = call, got == NULL) {                                                          \
      fprintf(stderr, "fail at %s:%d: %s, got %p==NULL\n", __FILE__, __LINE__, #call, got); \
      fail++;                                                                               \
    } else {                                                                                \
      pass++;                                                                               \
    }                                                                                       \
  } while (0)

#define TEST_MESSAGE(t, call, want, ...)       \
  do {                                         \
    t got;                                     \
    if (got = call, got != want) {             \
      fprintf(stderr, __VA_ARGS__, got, want); \
      fail++;                                  \
    } else {                                   \
      pass++;                                  \
    }                                          \
  } while (0)

#define TESTRESULTS                                                                    \
  do {                                                                                 \
    if (fail == 0) {                                                                   \
      fprintf(stderr, "%s: PASS: %d tests pass.\n", __func__, pass);                   \
      return 0;                                                                        \
    } else {                                                                           \
      fprintf(stderr, "%s: FAIL: %d tests passed, %d failed\n", __func__, pass, fail); \
      failedresults++;                                                                 \
      return -1;                                                                       \
    }                                                                                  \
  } while (0)

#define TESTGLOBALSETUP int failedresults = 0
#define TESTFUNC(name)                         \
  int name(void) __attribute__((constructor)); \
  int name(void)

#define TESTMAINRESULTS                     \
  do {                                      \
    if (failedresults != 0) {               \
      fprintf(stderr, "Tests failed!\n");   \
      return 1;                             \
    } else {                                \
      fprintf(stderr, "All tests pass.\n"); \
      return 0;                             \
    }                                       \
  } while (0)
#define TESTMAIN \
  int main(int argc, char **argv) { TESTMAINRESULTS; }
