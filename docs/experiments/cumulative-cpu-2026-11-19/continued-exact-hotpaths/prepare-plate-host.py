#!/usr/bin/env python3
"""Derive a separate certified 64-frame plate workload without changing frozen fixtures."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("missing or ambiguous fixture anchor: " + old)
    return text.replace(old, new)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("checkout", "sdk", "valgrind_includes", "output"):
        parser.add_argument(field, type=Path)
    args = parser.parse_args()
    checkout, sdk, includes, out = (path.resolve() for path in
                                    (args.checkout, args.sdk, args.valgrind_includes, args.output))
    out.mkdir()
    docs = checkout / "docs/experiments"
    fixture = docs / "heterogeneous-source-cutoff/native-short/guest-host.cpp"
    if hashlib.sha256(fixture.read_bytes()).hexdigest() != (
            "4d2df17fd14fffc0466f7fa7e2ca20f4d1d162abf8efc94c11774921e403100a"):
        raise RuntimeError("frozen guest fixture changed")
    source = fixture.read_text()
    scene = 'scene=="varied-held-short"||scene=="varied-wake-short"'
    source = replace_once(source, scene, scene + '||scene=="plate-held"')
    source = replace_once(source, "Instance i(module,factory8::controls(),0,0);",
                          """auto controls = factory8::controls();
    if (scene == "plate-held") { controls[cs80::P_fx1Type] = 4.f; controls[cs80::P_fx1Model] = 0.f; controls[cs80::P_fx1Mix] = .77f; }
    Instance i(module,controls,0,0);""")
    generated = out / "guest-plate.cpp"
    generated.write_text(source)
    harness = Path(__file__).resolve().parent.parent / "resumed-exact-hotpaths/harness"
    work = replace_once((harness / "work.py").read_text(),
                        "'varied-wake-short')", "'varied-wake-short','plate-held')")
    (out / "work-plate.py").write_text(work)
    driver = out / "guest-plate"
    subprocess.run([os.environ.get("CXX", "c++"), "-O3", "-DNDEBUG", "-std=gnu++20",
                    *(f"-I{p}" for p in (includes, checkout / "src", checkout / "tests",
                        docs / "eight-stage-filter/native-cost/host", sdk)), str(generated),
                    *(str(sdk / p) for p in ("pluginterfaces/base/coreiids.cpp",
                        "pluginterfaces/base/funknown.cpp", "public.sdk/source/vst/vstinitiids.cpp")),
                    "-ldl", "-o", str(driver)], check=True)
    print(hashlib.sha256(driver.read_bytes()).hexdigest())

if __name__ == "__main__":
    main()
