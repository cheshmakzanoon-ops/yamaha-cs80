#!/usr/bin/env python3
"""Select one failed GUI gate's log; never authorize another validation attempt.

Only fixed filenames can be returned. Randomized GUI takes precedence if both
failed. Unknown outcomes and cancellation fail closed without diagnostic replay.
"""
import sys

OUTCOMES = frozenset(("", "success", "failure", "skipped", "cancelled"))


def select_log(gui_outcome, regression_outcome):
    outcomes = (gui_outcome, regression_outcome)
    if any(value not in OUTCOMES for value in outcomes) or "cancelled" in outcomes:
        return None
    if gui_outcome == "failure":
        return "pluginval-gui.log"
    if regression_outcome == "failure":
        return "pluginval-gui-seed-regression.log"
    return None


def main():
    if len(sys.argv) != 3:
        print("Expected two GUI gate outcomes; no diagnostic selected.", file=sys.stderr)
        return 0
    if any(value not in OUTCOMES for value in sys.argv[1:]):
        print("Unrecognized GUI gate outcome; no diagnostic selected.", file=sys.stderr)
    selected = select_log(*sys.argv[1:])
    if selected:
        print(selected)
    return 0


if __name__ == "__main__":
    sys.exit(main())
