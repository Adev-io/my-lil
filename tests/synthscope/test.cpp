// test.cpp: checks the core with plain asserts. Run: make test
// Author: Ari Klopfer / A_Development. MIT
#include "check.h"
#include "test_engine.h"
#include "test_render.h"

int main()
{
    testCore();
    testParse();
    testBuild();
    testArt();
    testPresets();
    testRender();
    std::printf ("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
