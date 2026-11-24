#include "core/Engine.h"
#include <cstdio>
int main() { std::printf("Engine=%zu InstrumentBody=%zu\n", sizeof(cs80::Engine), sizeof(cs80::body::InstrumentBody)); }
