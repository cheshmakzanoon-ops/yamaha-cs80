#include "core/SteinbergAutomationCursor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <span>
#include <sstream>
#include <string>

namespace vst = cs80::vst3_automation;

// This test process counts every C++ heap allocation. Only reset/sampleAt's
// exact read-only section is measured; loading the JSON and printing failures
// are deliberately outside that section.
std::atomic<unsigned long long> allocationCount { 0 };
void* operator new(std::size_t bytes)
{
    allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (auto* result = std::malloc(bytes)) return result;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void* operator new[](std::size_t bytes)
{
    allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (auto* result = std::malloc(bytes)) return result;
    throw std::bad_alloc();
}
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace
{
int failures = 0;
void check(bool value, const char* what)
{
    if (! value) { ++failures; std::printf("FAIL: %s\n", what); }
}
void near(double value, double expected, double tolerance, const char* what)
{
    check(std::isfinite(value) && std::abs(value - expected) <= tolerance, what);
}
double gate(const std::string& json, const char* key)
{
    const auto name = '"' + std::string(key) + '"';
    const auto at = json.find(name);
    if (at == std::string::npos) { std::printf("Missing declared gate %s\n", key); std::exit(2); }
    return std::strtod(json.c_str() + json.find(':', at) + 1, nullptr);
}

struct Fixture
{
    std::span<const vst::Point> points;
    mutable int reads = 0;
    int failAtIndex = -1;

    static bool read(const void* context, int index, vst::Point& point) noexcept
    {
        const auto& fixture = *static_cast<const Fixture*>(context);
        ++fixture.reads;
        if (index == fixture.failAtIndex || index < 0 || index >= (int) fixture.points.size())
            return false;
        point = fixture.points[(std::size_t) index];
        return true;
    }
    vst::PointReader reader() const noexcept { return { this, (int) points.size(), read }; }
};

void explicitRamp(double tol)
{
    constexpr std::array points { vst::Point {0, 0.8}, vst::Point {128, 0.2} };
    Fixture fixture { points };
    vst::PiecewiseLinearCursor cursor;
    cursor.reset(fixture.reader(), 0.62, 256);
    for (int sample = 0; sample < 256; ++sample)
    {
        const double expected = sample <= 128 ? 0.8 + (0.2 - 0.8) * sample / 128.0 : 0.2;
        near(cursor.sampleAt(sample), expected, tol, "explicit linear ramp at each host sample");
    }
    check(cursor.status() == vst::Status::ok && fixture.reads == 2,
          "explicit ramp cursor reads each queue point exactly once");
}

void implicitPrevious(double tol)
{
    constexpr std::array points { vst::Point {128, 0.2} };
    Fixture fixture { points };
    vst::PiecewiseLinearCursor cursor;
    cursor.reset(fixture.reader(), 0.62, 256);
    for (int sample = 0; sample < 256; ++sample)
    {
        const double expected = sample <= 128
            ? 0.62 + (0.2 - 0.62) * (sample + 1) / 129.0 : 0.2;
        near(cursor.sampleAt(sample), expected, tol, "implicit previous VST3 point at sample minus one");
    }
    check(cursor.status() == vst::Status::ok && fixture.reads == 1,
          "implicit-point cursor fetches the only explicit point once");
}

void legalJump(double tol)
{
    constexpr std::array points { vst::Point {0, 0.8}, vst::Point {127, 0.8},
                                   vst::Point {128, 0.2} };
    Fixture fixture { points };
    vst::PiecewiseLinearCursor cursor;
    cursor.reset(fixture.reader(), 0.62, 256);
    for (int sample = 0; sample < 256; ++sample)
        near(cursor.sampleAt(sample), sample < 128 ? 0.8 : 0.2,
             tol, "legal adjacent-sample VST3 jump");
    check(cursor.status() == vst::Status::ok && fixture.reads == 3,
          "legal jump consumes three points once");
}

void repeatedOffset(double tol)
{
    constexpr std::array points { vst::Point {0, 0.8}, vst::Point {0, 0.75},
                                   vst::Point {128, 0.25} };
    Fixture fixture { points };
    vst::PiecewiseLinearCursor cursor;
    cursor.reset(fixture.reader(), 0.62, 256);
    for (int sample = 0; sample < 256; ++sample)
    {
        const double expected = sample <= 128
            ? 0.75 + (0.25 - 0.75) * sample / 128.0 : 0.25;
        near(cursor.sampleAt(sample), expected, tol, "repeated-offset last point wins without division by zero");
    }
    check(cursor.status() == vst::Status::ok && fixture.reads == 3,
          "repeated-offset cursor fetches three points once");
}

void emptyAndDense(double tol, int maxReaderCalls, int maxObjectBytes)
{
    check(sizeof(vst::PiecewiseLinearCursor) <= (std::size_t) maxObjectBytes,
          "cursor object respects preregistered fixed-space budget");
    vst::PiecewiseLinearCursor empty;
    empty.reset({}, 0.37, 256);
    for (int sample = 0; sample < 256; ++sample)
        near(empty.sampleAt(sample), 0.37, tol, "empty queue holds previous sample's value");
    check(empty.status() == vst::Status::ok && empty.readerCalls() == 0,
          "empty queue does not access an absent reader");

    const auto densePoints = []
    {
        std::array<vst::Point, 512> result {};
        for (int i = 0; i < 512; ++i)
            result[(std::size_t) i] = { i, 0.2 + 0.6 * i / 511.0 };
        return result;
    }();
    Fixture dense { densePoints };
    vst::PiecewiseLinearCursor cursor;
    const auto before = allocationCount.load(std::memory_order_relaxed);
    cursor.reset(dense.reader(), 0.1, 512);
    bool exact = true;
    for (int sample = 0; sample < 512; ++sample)
    {
        const double expected = 0.2 + 0.6 * sample / 511.0;
        const double rendered = cursor.sampleAt(sample);
        exact = exact && std::isfinite(rendered) && std::abs(rendered - expected) <= tol;
    }
    const auto newAllocations = allocationCount.load(std::memory_order_relaxed) - before;
    check(exact, "dense 512-point ramp matches every sample without 256-point truncation");
    check(newAllocations == 0, "dense queue cursor makes no heap allocations");
    check(cursor.status() == vst::Status::ok && dense.reads <= maxReaderCalls,
          "dense queue reader calls bounded by point count, not sample count squared");
}

void invalidQueues()
{
    constexpr std::array unsorted { vst::Point {128, 0.2}, vst::Point {64, 0.8} };
    Fixture fixture { unsorted };
    vst::PiecewiseLinearCursor cursor;
    cursor.reset(fixture.reader(), 0.62, 256);
    cursor.sampleAt(128);
    check(cursor.status() == vst::Status::invalidOrder,
          "out-of-order host points fail with a declared status");

    constexpr std::array negative { vst::Point {-1, 0.3} };
    Fixture badPoint { negative };
    cursor.reset(badPoint.reader(), 0.62, 256);
    check(cursor.status() == vst::Status::invalidPoint,
          "negative queue offset rejected without accessing invalid memory");

    constexpr std::array one { vst::Point {0, 0.8} };
    Fixture brokenReader { one, 0, 0 };
    cursor.reset(brokenReader.reader(), 0.62, 256);
    check(cursor.status() == vst::Status::invalidPoint,
          "failing external queue accessor reported instead of silent reuse");

    Fixture okay { one };
    cursor.reset(okay.reader(), 0.62, 256);
    cursor.sampleAt(128);
    cursor.sampleAt(127);
    check(cursor.status() == vst::Status::invalidSample,
          "host cursor requires monotonically advancing sample index");
}

void normalizeKeyParameters(double tol)
{
    for (auto value : {0.0, 0.5, 1.0})
    {
        near(vst::normalizedToCore(vst::Key::volume, value), value, tol,
             "volume normalized range is 0..1 with skew one");
        near(vst::normalizedToCore(vst::Key::brilliance, value), -2.0 + 4.0 * value, tol,
             "brilliance normalized range is -2..+2 with skew one");
        near(vst::normalizedToCore(vst::Key::resonance, value), value, tol,
             "resonance normalized range is 0..1 with skew one");
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s preregistered-steinberg-cursor.json\n", argv[0]); return 2; }
    std::ifstream input(argv[1]);
    check(input.good(), "preregistered Steinberg cursor JSON exists");
    std::ostringstream text;
    text << input.rdbuf();
    const auto truth = text.str();
    check(truth.find("steinberg-vst3-piecewise-linear-automation-cursor-v1") != std::string::npos,
          "correct preregistered Steinberg model ID");
    check(truth.find("alternatives_not_selected") != std::string::npos,
          "duplicate-offset inference alternatives are explicit");

    const double tolerance = gate(truth, "maximum_absolute_error");
    const int maxReaderCalls = (int) gate(truth, "dense_reader_calls_maximum");
    const int maxObjectBytes = (int) gate(truth, "cursor_object_size_bytes_maximum");
    const int allowedAllocations = (int) gate(truth, "cursor_dynamic_allocations");
    check(tolerance > 0 && tolerance <= 1e-12 && maxReaderCalls == 512
          && maxObjectBytes <= 96 && allowedAllocations == 0,
          "predeclared numerical, read-count, memory, allocation gates cannot loosen");

    explicitRamp(tolerance);
    implicitPrevious(tolerance);
    legalJump(tolerance);
    repeatedOffset(tolerance);
    emptyAndDense(tolerance, maxReaderCalls, maxObjectBytes);
    invalidQueues();
    normalizeKeyParameters(tolerance);

    std::printf("Steinberg VST3 cursor: %s (%d failures, %zu-byte cursor)\n",
                failures == 0 ? "PASS" : "FAIL", failures, sizeof(vst::PiecewiseLinearCursor));
    return failures == 0 ? 0 : 1;
}
