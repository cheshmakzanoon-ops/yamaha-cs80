#pragma once
#include <cstdint>
// No core header crosses this bridge. Each implementation is compiled against
// its own immutable tree in a distinct namespace, avoiding ODR/header mixing.
struct BenchConfig { double rate; int block, scene, moving, voices; bool retired; };
struct BenchInspection
{
    int active = 0;
    bool effective = false, finite = true;
    std::uint64_t samples = 0, rejected = 0;
    double stateChecksum = 0, filterEnvelopeChecksum = 0;
};
struct BenchApi
{
    void* (*create)(BenchConfig);
    void (*destroy)(void*);
    void (*callback)(void*,int,float*,const float*);
    BenchInspection (*inspect)(void*);
};
BenchApi baselineApi();
BenchApi candidateApi();
