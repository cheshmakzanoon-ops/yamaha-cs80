#!/usr/bin/env python3
"""Compile the actual preview signature arithmetic without the GUI.

The only JUCE symbol used by these functions is its binary32 pi constant,
supplied by an inert shim. This is a numerical boundary regression, not a
rendering, processor-restore, native-window or DAW qualification.
--source also accepts a saved baseline PresetBrowser.cpp as a negative control.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=ROOT / 'src/plugin/PresetBrowser.cpp')
parser.add_argument('--compiler', default='c++')
args = parser.parse_args()
source = args.source.read_text()


def definition(anchor):
    """Fail closed if a named definition disappears or changes shape."""
    start = source.index(anchor)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


shape = definition('struct PresetSignature\n') + ';'
tags = re.search(r'enum StyleTag : int \{[^}]+\};', source)
if tags is None:
    raise RuntimeError('StyleTag declaration not found')
parts = [definition('float unit01(float x)'), definition('float logNorm(float t)'),
         definition('float filterMagnitudeDb(const PresetSignature::Channel& ch, float hz)\n{'),
         definition('PresetSignature computeSignature(const cs80::ParamValues& v)')]
prefix = r'''
#include "core/FactoryPresets.h"
#include "plugin/PresetPreviewMath.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
namespace juce {
template<class T> struct MathConstants { static constexpr T pi = T(3.14159265358979323846); };
}
static_assert(std::bit_cast<std::uint32_t>(juce::MathConstants<float>::pi) == 0x40490fdbu);
namespace cs80::ui {
'''
test = r'''
} // namespace cs80::ui
int main() {
    using namespace cs80;
    int cases = 0, checks = 0, failures = 0;
    auto run = [&](const ParamValues& values) {
        ++cases;
        const auto s = ui::computeSignature(values);
        auto finite = [&](float value) {
            ++checks;
            if (!std::isfinite(value)) {
                if (failures < 10) std::fprintf(stderr, "Non-finite signature field in case %d\n", cases);
                ++failures;
            }
        };
        for (const auto& ch : s.ch) {
            for (float x : {ch.attack, ch.decay, ch.sustain, ch.release, ch.gain,
                            ch.hpf, ch.lpf, ch.hpfRes, ch.lpfRes, ch.saw, ch.pulse,
                            ch.sine, ch.noise, ch.markA, ch.markD, ch.markH, ch.markEnd}) finite(x);
            for (float x : ch.env) finite(x);
        }
        for (float x : s.radar) finite(x);
        for (float x : {s.cutoff, s.resonance, s.attack, s.release, s.sustain, s.decay,
                        s.chorus, s.layerOne, s.detune, s.lfo, s.pwm, s.ring}) finite(x);
        for (const auto& trace : s.trace) for (float x : trace) finite(x);
    };
    run(ParamValues{});
    for (const auto& preset : programLibrary()) run(preset.values);
    for (int p = 0; p < P_numParams; ++p) {
        ParamValues values;
        values[p] = paramDef(p).min; run(values);
        values[p] = paramDef(p).max; run(values);
    }
    std::uint32_t rng = 0x7f218b93u;
    for (int n = 0; n < 2000; ++n) {
        ParamValues values;
        for (int p = 0; p < P_numParams; ++p) {
            rng = 1664525u * rng + 1013904223u;
            const auto& d = paramDef(p);
            const float fraction = float(rng >> 8) / 16777215.0f;
            values[p] = d.min + (d.max - d.min) * fraction;
        }
        run(values);
    }
    ParamValues endpoint;
    for (int c = 0; c < 2; ++c) {
        endpoint[chParam(c, CP_vA)] = 0x1.11815cp+4f;
        endpoint[chParam(c, CP_vD)] = 0x1.2dddbcp+4f;
        endpoint[chParam(c, CP_vS)] = 1.0f;
        endpoint[chParam(c, CP_vR)] = 0x1.abbf1p+2f;
    }
    run(endpoint);
    std::printf("Extracted signature math: %d cases, %d finite checks, %d failures\n", cases, checks, failures);
    return failures == 0 ? 0 : 1;
}
'''
with tempfile.TemporaryDirectory(prefix='cs80-signature-') as directory:
    path = Path(directory)
    cpp = path / 'probe.cpp'
    cpp.write_text(prefix + shape + '\n' + tags.group() + '\n' + '\n'.join(parts) + test)
    binary = path / 'probe'
    subprocess.run([args.compiler, '-std=c++20', '-O2', '-fno-fast-math',
                    '-fsanitize=undefined,float-cast-overflow', '-fno-sanitize-recover=all',
                    '-I' + str(ROOT / 'src'), str(cpp), str(ROOT / 'src/core/FactoryPresets.cpp'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
