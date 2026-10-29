#!/usr/bin/env python3
"""Return only a validated seed for pluginval v1.0.4 --random-seed.
Last seed marker belongs to the final strict-wrapper attempt. A malformed final
marker must not fall back to an earlier attempt's seed.
"""
import re
import sys
from pathlib import Path


def select_seed(text):
    markers = [line.strip() for line in text.splitlines()
               if line.strip().startswith("Random seed:")]
    if not markers:
        return None
    match = re.fullmatch(r"Random seed:\s*(0x[0-9a-fA-F]{1,16}|[0-9]{1,20})", markers[-1])
    if not match:
        return None
    token = match.group(1)
    value = int(token, 16 if token.startswith("0x") else 10)
    # Zero asks the runner to choose a new seed, not replay zero.
    return hex(value) if 0 < value <= 0xffffffffffffffff else None


def main():
    try:
        path = Path(sys.argv[1])
        if path.stat().st_size > 8 * 1024 * 1024:
            raise ValueError("log exceeds diagnostic read bound")
        seed = select_seed(path.read_text(errors="replace"))
    except (OSError, ValueError, IndexError):
        seed = None
    if seed:
        print(seed)
    else:
        print("Original seed unavailable; rerun uses a different/unmatched seed.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
