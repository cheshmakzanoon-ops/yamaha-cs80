#pragma once
#include "KeyboardEventQueue.h"

namespace cs80
{
// Synchronous producer-only capture around JUCE's outer mouse dispatch. The
// key hooks precede JUCE noteOn; only the actual listener publication can bind
// the gesture. Top-scope-only, processor-specific TLS excludes unrelated
// threads and nested foreign dispatch, and restores nesting even on unwind.
class KeyboardGestureCapture
{
public:
    KeyboardGestureCapture(const void* processor, int channel, int note) noexcept
        : previous(top), owner(processor), expectedChannel(channel), expectedNote(note) { top = this; }
    ~KeyboardGestureCapture() { top = previous; }
    KeyboardGestureCapture(const KeyboardGestureCapture&) = delete;
    KeyboardGestureCapture& operator=(const KeyboardGestureCapture&) = delete;
    KeyPressureToken result() const noexcept { return token; }
    static void attempted(const void* processor, int channel, int note, KeyPressureToken accepted) noexcept
    {
        if (top == nullptr || top->owner != processor || top->expectedChannel != channel
            || top->expectedNote != note || top->sawAttempt) return;
        top->sawAttempt = true; // A rejected first attempt must not be substituted.
        top->token = accepted;
    }
private:
    inline static thread_local KeyboardGestureCapture* top = nullptr;
    KeyboardGestureCapture* previous;
    const void* owner;
    int expectedChannel, expectedNote;
    bool sawAttempt = false;
    KeyPressureToken token {};
};
}
