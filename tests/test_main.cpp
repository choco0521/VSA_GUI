// Single translation unit that defines doctest's main().
// All other tests are linked into the same executable and only include
// doctest.h normally.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
