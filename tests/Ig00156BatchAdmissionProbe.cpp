// Admission only: unsafe scalar compiler profiles have no bitwise-arithmetic
// promise. They must never enter the optional backend or a supplied callback.
#include "core/Ig00156Batch.h"
#include <cstdio>
#ifndef CS80_EXPECT_CALLER_BUILD
#error Set an independent caller-admission expectation.
#endif
#ifndef CS80_EXPECT_BACKEND_BUILD
#error Set an independent backend-admission expectation.
#endif
namespace {
unsigned callbackCalls = 0;
bool callback(const cs80::Ig00156Batch::Stages&, const cs80::Ig00156Batch::Inputs&,
              cs80::Ig00156Batch::Outputs&) noexcept
{
    ++callbackCalls;
    return true;
}
int failures = 0;
void check(bool value, const char* message)
{
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
bool avx2Available()
{
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}
}
int main()
{
#if defined(__FMA__) && defined(__x86_64__)
    __builtin_cpu_init();
    if (!__builtin_cpu_supports("fma")) return 77; // caller itself requires FMA
#endif
    using Batch = cs80::Ig00156Batch;
    check(CS80_IG00156_EXACT_BATCH_BUILD == CS80_EXPECT_CALLER_BUILD,
          "caller build admission matches the independent profile expectation");
    const auto kernel = Batch::selectKernel();
    const bool expectedKernel = CS80_EXPECT_BACKEND_BUILD && avx2Available();
    check((kernel != nullptr) == expectedKernel, "backend selects only an admitted CPU/build");
    check(Batch::selectKernel(false) == nullptr, "explicit disable remains scalar");
    std::array<cs80::Ig00156Stage, 4> stages;
    for (auto& stage : stages) stage.set(4000.f, 1.f, 96000.f);
    Batch::Stages pointers { &stages[0], &stages[1], &stages[2], &stages[3] };
    Batch::Inputs inputs { .125f, -.25f, .5f, -.75f };
    Batch::Outputs outputs;
    const bool used = cs80::processIg00156Four(pointers, inputs, outputs, kernel);
    const bool expectedUse = CS80_EXPECT_CALLER_BUILD && expectedKernel && Batch::environmentSupported();
    check(used == expectedUse, "actual backend use matches admission");
    // An incompatible caller must ignore even a non-null backend supplied by
    // another, strict translation unit. This test does not depend on AVX2 CPU.
    const bool called = cs80::processIg00156Four(pointers, inputs, outputs, &callback);
    check(called == bool(CS80_EXPECT_CALLER_BUILD), "caller gates a supplied callback");
    check(callbackCalls == unsigned(CS80_EXPECT_CALLER_BUILD), "callback not entered by an incompatible caller");
    std::printf("admission: caller=%d kernel=%d accelerated=%d callback=%u failures=%d\n",
                CS80_IG00156_EXACT_BATCH_BUILD, kernel != nullptr, used, callbackCalls, failures);
    return failures ? 1 : 0;
}
