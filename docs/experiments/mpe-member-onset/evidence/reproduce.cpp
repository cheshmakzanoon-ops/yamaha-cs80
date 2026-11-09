#include "core/Engine.h"
#include <cstdio>
using namespace cs80;
int main() {
  for (int circuit : {0, 1}) {
    Engine before, after;
    for (auto* e : {&before, &after}) {
      ParamValues p; p[P_sourcePerformance] = float(circuit); p[P_mpeOn] = 1.0f;
      p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = 0.0f;
      e->prepare(48000.0); e->setParams(p);
    }
    auto expression = [](Engine& e) { e.pitchBend(2, .25f); e.channelPressure(2, .7f); e.controlChange(74, .8f, 2); };
    expression(before); before.noteOn(60, .8f, 2);
    after.noteOn(60, .8f, 2); expression(after);
    const auto a = before.getVoiceInfo(0), b = after.getVoiceInfo(0);
    std::printf("policy=%d before: bend=%g pressure=%g; same-frame after: bend=%g pressure=%g\n", circuit, a.bend, a.pressure, b.bend, b.pressure);
  }
}
