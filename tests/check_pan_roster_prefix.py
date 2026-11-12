#!/usr/bin/env python3
"""Compile only the pan roster guards, without running a plugin/protocol suite."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'tests/Vst3PanAutomationTests.h').read_text()
start = source.index('    constexpr std::array originalKeys {')
end = source.index('    for (bool sixteen', start)
guards = source[start:end].replace('cs80::kSampleAccurateParameters', 'roster')
program = r'''
#include "plugin/HostAutomationBridge.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void check(bool ok, const char* message) { require(ok, message); }
void validate(const std::vector<int>& roster) {
''' + guards + r'''
}
int main() {
    const std::vector<int> current(cs80::kSampleAccurateParameters.begin(), cs80::kSampleAccurateParameters.end());
    int cases = 0;
    auto expect = [&](const char* name, const std::vector<int>& roster, bool accepted) {
        bool actual = true;
        try { validate(roster); } catch (const std::runtime_error&) { actual = false; }
        if (actual != accepted) throw std::runtime_error(name);
        std::cout << "PASS " << name << '\n'; ++cases;
    };
    expect("current production roster", current, true);
    auto prefix = std::vector<int>(current.begin(), current.begin() + 20);
    expect("historical twenty-lane prefix", prefix, true);
    auto extended = current;
    extended.push_back(cs80::P_ringDepth);
    expect("appended RING depth preserves prefix", extended, true);
    extended.push_back(cs80::P_ringAttack);
    extended.push_back(cs80::P_ringDecay);
    expect("appended RING times preserve prefix", extended, true);
    auto shortRoster = prefix; shortRoster.pop_back();
    expect("missing second pan lane rejected", shortRoster, false);
    auto reordered = prefix; std::swap(reordered[0], reordered[1]);
    expect("original order change rejected", reordered, false);
    auto swappedPan = current; std::swap(swappedPan[18], swappedPan[19]);
    expect("pan order change rejected", swappedPan, false);
    auto displacedPan = current; std::swap(displacedPan[19], displacedPan[20]);
    expect("pan displacement rejected", displacedPan, false);
    std::cout << cases << " focused roster checks passed; production lanes=" << current.size() << '\n';
}
'''
with tempfile.TemporaryDirectory(prefix='cs80-pan-roster-') as tmp:
    cpp = Path(tmp) / 'check.cpp'
    exe = Path(tmp) / 'check'
    cpp.write_text(program)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I', str(root / 'src'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
