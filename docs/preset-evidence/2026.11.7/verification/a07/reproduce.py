#!/usr/bin/env python3
"""Build the current PCM suite and the exact pre-A07 negative control."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[4]
receipt = json.loads((here / "receipt.json").read_text())
header = root / "src/core/InstrumentBody.h"
assert hashlib.sha256(header.read_bytes()).hexdigest() == receipt["current_header_sha256"], \
    "Source changed after this measurement; compare the receipt before reproducing."
with tempfile.TemporaryDirectory(prefix="cs80-a07-") as directory:
    temporary = Path(directory)
    before = temporary / "before"
    (before / "core").mkdir(parents=True)
    for name in ("InstrumentBody.h", "InstrumentBodyVariants.h", "InstrumentBodyMaterials.h"):
        shutil.copyfile(root / "src/core" / name, before / "core" / name)
    subprocess.run(["patch", "-p1", "-d", str(before), "-i", str(here / "before-a07.patch")], check=True)
    assert hashlib.sha256((before / "core/InstrumentBody.h").read_bytes()).hexdigest() \
        == receipt["before_header_sha256"]
    for label, include in (("current", root / "src"), ("before", before)):
        executable = temporary / (label + "-tests")
        subprocess.run(["g++", "-O2", "-std=c++20", "-I", str(include),
                        str(root / "tests/LowChoirSpectrumTests.cpp"), "-o", str(executable)], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        print(label, result.returncode, result.stdout, sep="\n")
        assert result.returncode == (0 if label == "current" else 1)
    # Preserve both implementations in one process for an informational
    # interleaved benchmark. A11 owns complete combined CPU qualification.
    renamed = temporary / "paired-baseline"
    renamed.mkdir()
    for source in (before / "core").glob("*.h"):
        (renamed / source.name).write_text(source.read_text().replace("namespace cs80::body", "namespace cs80old::body"))
    executable = temporary / "paired-benchmark"
    subprocess.run(["g++", "-O3", "-DNDEBUG", "-std=c++20", "-I", str(root / "src"),
                    "-I", str(temporary), str(here / "paired-benchmark.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
