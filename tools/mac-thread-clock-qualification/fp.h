#pragma once
#if !defined(__APPLE__) || !defined(__aarch64__)
#error Native Apple ARM required
#endif
#include <AvailabilityMacros.h>
#if !defined(MAC_OS_X_VERSION_MIN_REQUIRED) || MAC_OS_X_VERSION_MIN_REQUIRED < 101200
#error Deployment target must support public clock_gettime API
#endif
#include <cstdint>
#include <fstream>
struct FP { uint64_t control, status; };
inline FP fp() { FP f; asm volatile("mrs %0, fpcr" : "=r"(f.control)); asm volatile("mrs %0, fpsr" : "=r"(f.status)); return f; }
inline bool recordFP(const char* path, FP a, FP b) { std::ofstream f(path); f << "{\"fpcr_before\":" << a.control << ",\"fpcr_after\":" << b.control << ",\"fpsr_before\":" << a.status << ",\"fpsr_after\":" << b.status << "}\n"; return bool(f) && a.control == b.control; }
