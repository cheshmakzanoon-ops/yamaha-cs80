#pragma once
#include <cstdint>
// No production header or object layout crosses this boundary.
struct OffConfig { int rate, block, scene, condition, moving, policy; };
struct OffInput { float wave; int callback; };
struct OffInspection {
    int active = 0, policy = 0;
    bool effective = false, finite = true;
    std::uint64_t samples = 0, rejected = 0;
    double stateChecksum = 0.0;
};
struct OffApi {
    void* (*create)(OffConfig);
    void (*destroy)(void*);
    void (*prepareInput)(void*, OffInput);
    bool (*callback)(void*, float*, const float*);
    OffInspection (*inspect)(void*);
};
OffApi offBaselineApi();
OffApi offCandidateApi();
