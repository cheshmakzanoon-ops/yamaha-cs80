#!/usr/bin/env python3
"""Exercise real preset preference controls and fresh-process persistence."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    failures = 0
    with tempfile.TemporaryDirectory(prefix="cs80-preset-preferences-") as tmp:
        root = Path(tmp)
        regular = root / "favourites.txt"
        missing = root / "new-folder" / "favourites.txt"
        blocked = root / "not-a-directory"
        blocked.write_text("unchanged blocking file\n")
        phases = [("enable", regular), ("read-enabled", regular),
                  ("disable", regular), ("read-disabled", regular),
                  ("missing", missing), ("read-missing", missing),
                  ("blocked", blocked / "favourites.txt")]
        for phase, path in phases:
            env = dict(os.environ, CS80_FAVOURITES_FILE=str(path))
            result = subprocess.run([str(args.executable.resolve()), "--preset-preferences-tests", phase],
                                    env=env, text=True, capture_output=True, timeout=20)
            print(f"{phase}: exit {result.returncode}", flush=True)
            print(result.stdout + result.stderr, end="", flush=True)
            if result.returncode != 0:
                failures += 1
        if blocked.read_text() != "unchanged blocking file\n":
            failures += 1
            print("FAIL: failed persistence modified the blocking fixture")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
