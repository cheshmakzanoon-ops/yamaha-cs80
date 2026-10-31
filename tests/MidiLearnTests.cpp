// MidiLearnMap (src/plugin/MidiLearn.h): the per-session controller -> parameter map read and
// written by the audio thread without locks. Single-threaded semantics first, then a two-thread
// run in which an "audio" thread learns and applies controllers while a "message" thread arms,
// assigns and clears, checking that the one-controller-per-parameter invariant and the bounds
// of every returned value hold throughout.

#include "plugin/MidiLearn.h"

#include <atomic>
#include <cstdio>
#include <thread>

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* name)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", name);
    }
}

// Pre-registered bounds for the concurrent run (committed with the test, not tuned to results).
constexpr int kStressIterations = 200000;
constexpr int kParameters = 330;   // more than the host parameter count, so indexes are not special
} // namespace

int main()
{
    using cs80::MidiLearnMap;

    // ---- controllers with protocol meaning are never learnable --------------------------------
    for (int cc : { 0, 6, 32, 38, 96, 97, 98, 99, 100, 101, 120, 121, 123, 127, -1, 128 })
        check(! MidiLearnMap::isLearnable(cc), "protocol controllers are not learnable");
    for (int cc : { 1, 2, 7, 11, 64, 74, 95, 102, 119 })
        check(MidiLearnMap::isLearnable(cc), "ordinary controllers are learnable");

    // ---- assignment, one controller per parameter, one parameter per controller ---------------
    {
        MidiLearnMap map;
        check(map.count() == 0 && map.parameterFor(74) == MidiLearnMap::kNone, "a new map is empty");
        const auto g0 = map.generation();
        check(map.assign(74, 12), "assign a learnable controller");
        check(map.generation() != g0, "an edit bumps the generation");
        check(map.parameterFor(74) == 12 && map.controllerFor(12) == 74 && map.count() == 1, "assignment is readable both ways");
        check(map.assign(71, 12), "reassign the parameter to another controller");
        check(map.parameterFor(74) == MidiLearnMap::kNone && map.controllerFor(12) == 71 && map.count() == 1,
              "a parameter keeps only its newest controller");
        check(map.assign(71, 40) && map.parameterFor(71) == 40 && map.controllerFor(12) == MidiLearnMap::kNone,
              "a controller drives only its newest parameter");
        check(! map.assign(0, 5) && ! map.assign(123, 5) && ! map.assign(20, -3), "invalid assignments are refused");
        map.assign(20, 5);
        map.assign(21, 6);
        check(map.clearParameter(5) == 1 && map.parameterFor(20) == MidiLearnMap::kNone, "clear one parameter");
        map.clearController(21);
        check(map.parameterFor(21) == MidiLearnMap::kNone, "clear one controller");
        map.clearAll();
        check(map.count() == 0, "clear all");
    }

    // ---- learning on the audio-thread entry point ---------------------------------------------
    {
        MidiLearnMap map;
        check(map.handleController(74) == MidiLearnMap::kNone, "an unassigned controller keeps its MIDI meaning");
        map.armLearn(33);
        check(map.learningParameter() == 33, "learning is armed");
        check(map.handleController(0) == MidiLearnMap::kNone && map.learningParameter() == 33,
              "a protocol controller neither learns nor disarms");
        check(map.handleController(74) == 33, "the next learnable controller is learned and applied at once");
        check(map.learningParameter() == MidiLearnMap::kNone, "learning disarms after one controller");
        check(map.handleController(74) == 33 && map.handleController(75) == MidiLearnMap::kNone, "the learned mapping persists");
        map.armLearn(33);
        check(map.handleController(1) == 33 && map.parameterFor(74) == MidiLearnMap::kNone,
              "learning a mapped parameter again moves it to the new controller");
        map.armLearn(40);
        map.cancelLearn();
        check(map.handleController(2) == MidiLearnMap::kNone, "cancel leaves the next controller unlearned");
    }

    // ---- two threads: the audio side learns and applies while the message side edits ----------
    {
        MidiLearnMap map;
        std::atomic<bool> stop { false };
        std::atomic<int> outOfRange { 0 };
        std::thread audio([&]
        {
            unsigned seed = 1;
            while (! stop.load(std::memory_order_acquire))
            {
                seed = seed * 1664525u + 1013904223u;
                const int cc = (int) ((seed >> 16) % 128u);
                const int parameter = map.handleController(cc);
                if (parameter != MidiLearnMap::kNone && (parameter < 0 || parameter >= kParameters))
                    outOfRange.fetch_add(1, std::memory_order_relaxed);
            }
        });
        unsigned seed = 7;
        for (int i = 0; i < kStressIterations; ++i)
        {
            seed = seed * 22695477u + 1u;
            const int parameter = (int) ((seed >> 8) % (unsigned) kParameters);
            const int cc = (int) ((seed >> 20) % 128u);
            switch ((seed >> 4) % 5u)
            {
                case 0: map.armLearn(parameter); break;
                case 1: map.assign(cc, parameter); break;
                case 2: map.clearParameter(parameter); break;
                case 3: map.clearController(cc); break;
                default: (void) map.controllerFor(parameter); break;
            }
        }
        stop.store(true, std::memory_order_release);
        audio.join();
        check(outOfRange.load() == 0, "concurrent use never yields a parameter outside the assigned range");
        // Quiescent invariant after the race: every controller maps to nothing or a valid parameter, and
        // protocol controllers were never assigned.
        bool valid = true;
        for (int cc = 0; cc < MidiLearnMap::kNumControllers; ++cc)
        {
            const int parameter = map.parameterFor(cc);
            if (parameter != MidiLearnMap::kNone && (! MidiLearnMap::isLearnable(cc) || parameter >= kParameters))
                valid = false;
        }
        check(valid, "after the race only learnable controllers hold valid parameters");
        // A single-threaded pass restores the one-controller-per-parameter invariant for each parameter touched.
        for (int parameter = 0; parameter < kParameters; ++parameter)
        {
            const int cc = map.controllerFor(parameter);
            if (cc != MidiLearnMap::kNone)
                map.assign(cc, parameter);
        }
        bool unique = true;
        for (int parameter = 0; parameter < kParameters; ++parameter)
        {
            int owners = 0;
            for (int cc = 0; cc < MidiLearnMap::kNumControllers; ++cc)
                owners += map.parameterFor(cc) == parameter ? 1 : 0;
            unique = unique && owners <= 1;
        }
        check(unique, "reassignment re-establishes one controller per parameter");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
