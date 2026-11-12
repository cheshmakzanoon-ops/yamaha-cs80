#include "MidiLearn.h"
#include <cstdio>

namespace {
int checks = 0, failures = 0;
void check(bool condition, const char* label)
{
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
struct Observer
{
    cs80::MidiLearnMap* map = nullptr;
    int calls = 0;
    int expectedCc = -1, expectedParameter = -1, expectedCount = 0;
    bool inspect = false;
    static void changed(void* context) noexcept
    {
        auto& self = *static_cast<Observer*>(context);
        ++self.calls;
        if (self.inspect)
        {
            check(self.map->count() == self.expectedCount, "callback sees final map count");
            if (self.expectedCc >= 0)
                check(self.map->parameterFor(self.expectedCc) == self.expectedParameter,
                      "callback sees final assignment");
        }
    }
};
}
int main()
{
    Observer observer;
    cs80::MidiLearnMap map(&observer, &Observer::changed);
    observer.map = &map;
    check(observer.calls == 0 && map.count() == 0, "construction silent and empty");
    map.armLearn(4); map.cancelLearn(); map.armLearn(-1);
    check(observer.calls == 0, "arming/canceling not persistent");
    map.clearAll(); map.clearController(7); map.clearController(-1); map.clearController(128);
    check(map.clearParameter(4) == 0, "absent parameter removes nothing");
    check(map.clearParameter(-1) == 0, "negative parameter removes nothing");
    check(observer.calls == 0, "empty/invalid clears silent");
    check(!map.assign(0, 4) && !map.assign(6, 4) && !map.assign(128, 4)
          && !map.assign(7, -1), "invalid assignments refused");
    check(observer.calls == 0, "invalid assignments silent");

    observer.inspect = true; observer.expectedCc = 7; observer.expectedParameter = 4;
    observer.expectedCount = 1;
    check(map.assign(7, 4), "valid assignment accepted");
    check(observer.calls == 1, "new assignment notifies once");
    check(map.assign(7, 4), "identical valid assignment still accepted");
    check(observer.calls == 1, "identical assignment silent");
    observer.expectedCc = 8;
    map.assign(8, 4);
    check(observer.calls == 2 && map.parameterFor(7) == -1, "moving mapping removes previous controller");
    observer.expectedParameter = 5;
    map.assign(8, 5);
    check(observer.calls == 3 && map.controllerFor(4) == -1, "destination displacement notifies once");
    observer.expectedCc = 9; observer.expectedParameter = 6; observer.expectedCount = 2;
    map.assign(9, 6);
    observer.expectedCc = 8; observer.expectedParameter = 6; observer.expectedCount = 1;
    map.assign(8, 6);
    check(observer.calls == 5 && map.parameterFor(9) == -1 && map.controllerFor(5) == -1,
          "move onto populated destination notifies once after complete mutation");
    observer.expectedParameter = -1; observer.expectedCount = 0;
    check(map.clearParameter(6) == 1, "clear parameter returns removed count");
    check(observer.calls == 6, "parameter removal notifies");
    map.clearParameter(6); map.clearParameter(-1);
    check(observer.calls == 6, "repeated/negative parameter removal silent");

    observer.expectedCc = 11; observer.expectedParameter = 12; observer.expectedCount = 1;
    map.armLearn(12);
    check(map.handleController(0) == -1 && map.learningParameter() == 12,
          "protocol CC does not consume armed learn");
    check(map.handleController(-1) == -1 && map.handleController(128) == -1,
          "out of range controller ignored");
    check(observer.calls == 6, "arming and ignored input silent");
    check(map.handleController(11) == 12 && map.learningParameter() == -1, "audio handle learns target");
    check(observer.calls == 7, "audio assignment notifies once");
    check(map.handleController(11) == 12 && observer.calls == 7, "ordinary mapped input silent");
    map.armLearn(12); map.handleController(11);
    check(observer.calls == 7 && map.learningParameter() == -1, "relearning identical pair silent");
    observer.expectedCc = 13;
    map.armLearn(12); map.handleController(13);
    check(observer.calls == 8 && map.parameterFor(11) == -1, "audio reassign removes previous controller");
    observer.expectedParameter = -1; observer.expectedCount = 0;
    map.clearController(13);
    check(observer.calls == 9, "controller removal notifies once");
    map.clearController(13);
    check(observer.calls == 9, "repeated controller removal silent");

    observer.inspect = false;
    map.assign(14, 15, false); map.assign(16, 17, false);
    check(map.count() == 2 && observer.calls == 9, "restore assignment suppression");
    map.armLearn(18);
    map.clearAll(false);
    check(map.count() == 0 && map.learningParameter() == -1 && observer.calls == 9,
          "restore clear suppresses notification and cancels learn");
    map.assign(14, 15, false); map.assign(16, 17, false);
    observer.inspect = true; observer.expectedCc = -1; observer.expectedCount = 0;
    map.clearAll();
    check(observer.calls == 10, "clear populated map emits one final-state notification");
    map.armLearn(18); map.clearAll();
    check(observer.calls == 10 && map.learningParameter() == -1, "clear armed-only state silent");
    observer.expectedCc = 19; observer.expectedParameter = 20; observer.expectedCount = 1;
    map.assign(19, 20);
    check(observer.calls == 11, "suppression is per operation, not sticky");

    cs80::MidiLearnMap unobserved;
    check(unobserved.assign(7, 1), "default constructor retains assignments");
    unobserved.clearAll();
    check(unobserved.count() == 0, "default map needs no callback");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
