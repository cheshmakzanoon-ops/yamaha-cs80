#!/usr/bin/env python3
"""Build isolated before/current Engine cores and reproduce the A10 comparison.

Needs a C++20 compiler, ar, git, Python and NumPy. No shared build or repository
source is edited. Audio, headers and binaries stay in --work outside the repo.
"""
import argparse
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

HERE = Path(__file__).resolve().parent


def run(command, **kwargs):
    subprocess.run([str(item) for item in command], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[4])
    parser.add_argument("--baseline", default="18711f93a39aa49f3d6f9a41784be456224facbe")
    parser.add_argument("--work", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    args.work = args.work or Path(tempfile.mkdtemp(prefix="cs80-a10-reproduce-"))
    args.output = args.output or args.work / "evidence"
    args.work.mkdir(parents=True, exist_ok=True)
    args.output.mkdir(parents=True, exist_ok=True)
    print(f"Temporary build/capture directory: {args.work}", flush=True)
    archive = subprocess.check_output(["git", "archive", args.baseline, "src"], cwd=args.repo)
    (args.work / "before").mkdir(exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        source.extractall(args.work / "before", filter="data")
    shutil.copytree(args.repo / "src", args.work / "after" / "src", dirs_exist_ok=True)

    def build_and_render(revision):
        root = args.work / revision
        source = root / "src"
        objects = []
        for name in ("Engine", "EngineWscHeld", "FactoryPresets", "Ig00156Batch"):
            obj = root / f"{name}.o"
            command = [args.compiler, "-std=gnu++20", "-O3", "-DNDEBUG", "-fPIC", "-I", source]
            if name in ("Engine", "EngineWscHeld"):
                command += ["-DCS80_ENABLE_BORROWED_HELD=1"]
            if name == "Ig00156Batch":
                command += ["-ffp-contract=off"]
            run(command + ["-c", source / "core" / f"{name}.cpp", "-o", obj])
            objects.append(obj)
        library = root / "libcs80_core.a"
        if library.exists():
            library.unlink()
        run(["ar", "rcs", library, *objects])
        run([args.compiler, "-std=c++20", "-O2", "-I", source, HERE / "engine_texture.cpp",
             library, "-o", root / "engine_texture"])
        with (args.output / f"{revision}_render.log").open("w") as log:
            run([root / "engine_texture", root / "pcm"], stdout=log)
        print(f"Completed {revision}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(build_and_render, ("before", "after")))
    provenance = {
        "baseline_commit": args.baseline,
        "compiler": subprocess.check_output([args.compiler, "--version"], text=True).splitlines()[0],
        "method": "separate source/header snapshots; CMake-equivalent core compilation flags",
        "files": {},
    }
    for revision in ("before", "after"):
        for relative in ("src/core/InstrumentBody.h", "src/core/Engine.h", "src/core/Engine.cpp", "libcs80_core.a"):
            path = args.work / revision / relative
            provenance["files"][f"{revision}/{relative}"] = {
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size}
    (args.output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    run([sys.executable, HERE / "analyze_texture.py", args.work, args.output])


if __name__ == "__main__":
    main()
