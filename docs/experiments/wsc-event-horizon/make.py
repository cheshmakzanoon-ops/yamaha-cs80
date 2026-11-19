#!/usr/bin/env python3
import pathlib,sys,hashlib
p=pathlib.Path(__file__).resolve().parent
src=pathlib.Path(sys.argv[1]); dst=pathlib.Path(sys.argv[2]); dst.mkdir(parents=True,exist_ok=True)
s=src.read_text(); assert hashlib.sha256(s.encode()).hexdigest()=='c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373'
# Both receive the same inert test friend, for fieldwise endpoint observations.
s=s.replace('    struct Nodes {','    friend struct HorizonObserver;\n    struct Nodes {',1)
(dst/'stock.h').write_text(s+'\n// Original branch oracle.\n')
s=s.replace('#include <algorithm>','#include <bit>\n#include <cstring>\n#if defined(__x86_64__)\n#include <xmmintrin.h>\n#endif\n#include <algorithm>',1)
s=s.replace('    bool endNodesValid=false;', '    bool endNodesValid=false;\n    unsigned char horizonRemaining=0; // adjacent to bool; no new allocation or pointer')
old='    WscOutputVolts tick(double requestedHz,const WscControls& controls={}) noexcept {'
assert s.count(old)==1
s=s.replace(old,(p/'interior.inc').read_text()+'    WscOutputVolts tickStock(double requestedHz,const WscControls& controls) noexcept {')
s=s.replace('    // Idle evolution of a silent circuit','public:\n    // Idle evolution of a silent circuit',1)
for start in ['void reset(double phase=0.,double initialHz=261.6) noexcept {','double advanceIdle(double seconds,double requestedHz,const WscControls& controls) noexcept {','void restoreState(const State& captured) noexcept {','void restoreIdleState(const IdleState& s) noexcept {','void copyFrom(const ReconstructedOscillator& other) noexcept {']:
 assert s.count(start)==1
 s=s.replace(start,start+' horizonRemaining=0;')
(dst/'candidate.h').write_text(s+'\n// Disjoint experimental copy.\n')
