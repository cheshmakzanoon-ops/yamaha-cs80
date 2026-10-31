// Registered in parity/tolerances-v2/au-wrapper-macro-environment.json.
// JUCE 8.0.11's juce_IncludeModuleHeaders.h defines both of these macros
// before the native macOS AU wrapper includes our queue. Reproduce that
// preprocessing environment on every host; native AU validation is separate.
#include <type_traits>

namespace juce
{
template <typename ValueType> class Point;
class Component;
}

#define Component juce::Component
#define Point juce::Point

#include "plugin/AuAutomationQueue.h"

#ifndef Point
 #error "The production header must preserve the caller's Point macro"
#endif
#ifndef Component
 #error "The production header must preserve the caller's Component macro"
#endif

using WrapperPoint = Point<float>;
using WrapperComponent = Component;

#undef Point
#undef Component

static_assert(std::is_same_v<WrapperPoint, juce::Point<float>>);
static_assert(std::is_same_v<WrapperComponent, juce::Component>);

int main()
{
    const cs80::AuAutomationQueue queue;
    return queue.available() == cs80::AuAutomationQueue::capacity ? 0 : 1;
}
