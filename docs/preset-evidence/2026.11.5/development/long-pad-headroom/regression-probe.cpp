#define main pad_poly_complete_suite_main
#include "tests/PadPolyRefinementTests.cpp"
#undef main
int main() {
    longHeldPadHeadroom();
    std::printf("Long-pad headroom: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
