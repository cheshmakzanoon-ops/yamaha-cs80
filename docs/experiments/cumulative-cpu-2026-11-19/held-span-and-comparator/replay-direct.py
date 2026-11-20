#!/usr/bin/env python3
"""Compare the pinned parent to the checkout. Compiler-specific direct study, not a CPU timer."""
import argparse
import os
import shutil
import subprocess
from pathlib import Path

DEFAULT_BASE = "9208eb6323207b5054055dd2e0549e45324c539a"
parser = argparse.ArgumentParser()
parser.add_argument("repo", type=Path)
parser.add_argument("new_work", type=Path)
parser.add_argument("--cxx", default="g++-14")
parser.add_argument("--optimization", choices=["O1","O2","O3","Og"], default="O3")
parser.add_argument("--mutation-check", action="store_true")
parser.add_argument("--baseline", default=DEFAULT_BASE, help="Git source revision for the stock oscillator")
args = parser.parse_args()
BASE = args.baseline
here = Path(__file__).resolve().parent
r = args.repo.resolve()
w = args.new_work.resolve()
w.mkdir(parents=True, exist_ok=False)
os.chdir(w)

def show(f):
    return subprocess.check_output(["git", "-C", str(r), "show", BASE + ":" + f], text=True)

for arm in ['stock','candidate']:
 for name in ['ReconstructedOscillator.h','EngineWscHeld.h','EngineWscHeld.cpp','WscActiveSegments.inc']:
  src=show('src/core/'+name) if arm=='stock' else (r/'src/core'/name).read_text()
  if name=='ReconstructedOscillator.h':src=src.replace('    friend struct EngineWscHeldAccess;', '    friend struct EngineWscHeldAccess; friend struct HorizonObserver;')
  p=Path('p'+arm)/'core'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(src+'\n// separate '+arm+' proof input\n')
# Older stock revisions inlined both bodies; newer ones share the include files.
stock_core = set(subprocess.check_output(["git","-C",str(r),"ls-tree","--name-only",BASE+":src/core"], text=True).splitlines())
for extra in ['WscAdvanceSegment.inc','WscNextBranchEvent.inc']:
 if extra in stock_core: (Path('pstock/core')/extra).write_text(show('src/core/'+extra))
 source = r/'src/core'/extra
 if source.is_file(): (Path('pcandidate/core')/extra).write_bytes(source.read_bytes())
s=show('docs/experiments/wsc-event-horizon/direct.cpp')
s=s.replace('"gen/stock.h"','"pstock/core/EngineWscHeld.h"').replace('"gen/candidate.h"','"pcandidate/core/EngineWscHeld.h"')
s=s.replace('uint64_t fast0=C::horizonFast,slow0=C::horizonSlow, grants0=C::horizonGrants;', 'uint64_t delta=0;')
s=s.replace('initFp(mx,cw);auto a=s.tick(f,sc);auto af=flags();initFp(mx,cw);auto d=c.tick(f,cc);auto df=flags();++pairs;', '''initFp(mx,cw);bool au=cs80::EngineWscHeldAccess::canBorrow(s,f,sc);
   auto a=au?cs80::EngineWscHeldAccess::run(s,f):s.tick(f,sc);auto af=flags();
   initFp(mx,cw);bool du=cs80_candidate::EngineWscHeldAccess::canBorrow(c,f,cc);
   auto d=du?cs80_candidate::EngineWscHeldAccess::run(c,f):c.tick(f,cc);auto df=flags();++pairs;
   ck(au==du,"matching dispatch",scene,i); delta+=du;''')
start=s.index('  auto delta=C::horizonFast');end=s.index(' }\n#ifdef CS80_WSC',start)
s=s[:start]+'''  totalFast+=delta;
  if(scene==10)ck(delta==0,"nondefault rounding must stay stock",scene,0);
  if(scene==0||scene==1)ck(delta>0,"positive activation",scene,0);
  std::printf("%s %d admitted=%llu\\n",labels[scene],n,(unsigned long long)delta);
'''+s[end:]
s=s.replace('failures=%lld skipped=%llu witnessDisagreements=%llu','failures=%lld admitted=%llu').replace(',failures,(unsigned long long)totalFast,(unsigned long long)seenFalse',',failures,(unsigned long long)totalFast')
Path('proof.cpp').write_text(s)
s=show('docs/experiments/wsc-event-horizon/boundaries.cpp').replace('"direct.cpp"','"proof.cpp"')
s=s.replace('initFp(0x1fa0);S s;C c;', 'uint64_t both=0;initFp(0x1fa0);S s;C c;')
s=s.replace('initFp(mx,cw);auto x=s.tick(f,sc);auto fx=flags();initFp(mx,cw);auto y=c.tick(f,cc);auto fy=flags();', '''initFp(mx,cw); bool aok=cs80::EngineWscHeldAccess::canBorrow(s,f,sc);
   auto x=aok?cs80::EngineWscHeldAccess::run(s,f):s.tick(f,sc);auto fx=flags();
   initFp(mx,cw); bool bok=cs80_candidate::EngineWscHeldAccess::canBorrow(c,f,cc);
   auto y=bok?cs80_candidate::EngineWscHeldAccess::run(c,f):c.tick(f,cc);auto fy=flags();
   ck(aok==bok,"matching dispatch",k,j);both+=bok;''')
s=s.replace('(unsigned long long)C::horizonFast','(unsigned long long)both')
Path('edges.cpp').write_text(s)

# Both tested objects use exactly the same observation-only friend declaration;
# the second namespace allows the two genuine borrowed entry points to be linked
# into one process with equal, independently reset FP environments and inputs.
cc = [args.cxx, "-" + args.optimization, "-DNDEBUG", "-std=gnu++20", "-fPIC", "-DCS80_ENABLE_BORROWED_HELD=1"]
def run(*argv):
    subprocess.run([*cc, *argv], check=True)
run("-c", "pstock/core/EngineWscHeld.cpp", "-o", "pstock.o")
run("-Dcs80=cs80_candidate", "-c", "pcandidate/core/EngineWscHeld.cpp", "-o", "pcandidate.o")
shutil.copyfile(here / "profiles.cpp", w / "profiles.cpp")
for program in ["proof", "edges", "profiles"]:
    run(program + ".cpp", "pstock.o", "pcandidate.o", "-o", program)
    proc = subprocess.run(["./" + program], stdout=subprocess.PIPE, text=True, check=True)
    Path(program + ".log").write_text(proc.stdout)
    print(proc.stdout, end="")


if args.mutation_check:
    # Only the disposable, generated receiver is corrupted. The reference
    # and both checkout files remain untouched.
    part = Path("pcandidate/core/WscActiveSegments.inc")
    old = "if (branchEvent) advance(span,start);"
    candidate = part.read_text()
    if candidate.count(old) != 1:
        raise RuntimeError("expected one prepared-duration fallback")
    part.write_text(candidate.replace(old, "if (false) advance(span,start);"))
    run("-Dcs80=cs80_candidate", "-c", "pcandidate/core/EngineWscHeld.cpp", "-o", "bad.o")
    run("proof.cpp", "pstock.o", "bad.o", "-o", "negative")
    proc = subprocess.run(["./negative"], capture_output=True, text=True)
    Path("negative.log").write_text(proc.stderr + proc.stdout)
    if proc.returncode != 1 or "FAIL three pins" not in proc.stderr:
        raise RuntimeError("unconditional full-duration substitution was not detected")
    print("Expected rejection:", proc.stdout.splitlines()[-1])
