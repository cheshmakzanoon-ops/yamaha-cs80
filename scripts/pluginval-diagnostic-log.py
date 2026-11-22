#!/usr/bin/env python3
"""Select one failed gate's log; never authorize another validation attempt.

Only fixed filenames can be returned. Existing randomized/fixed GUI precedence
is preserved, followed by headless. Unknown outcomes and cancellation fail
closed without diagnostic replay. Two-outcome callers remain supported.
"""
import sys

OUTCOMES = frozenset(("", "success", "failure", "skipped", "cancelled"))


def select_log(gui_outcome, regression_outcome, headless_outcome=""):
    outcomes = (gui_outcome, regression_outcome, headless_outcome)
    if any(value not in OUTCOMES for value in outcomes) or "cancelled" in outcomes:
        return None
    if gui_outcome == "failure":
        return "pluginval-gui.log"
    if regression_outcome == "failure":
        return "pluginval-gui-seed-regression.log"
    if headless_outcome == "failure":
        return "pluginval-headless.log"
    return None


def main():
    if len(sys.argv) not in (3, 4):
        print("Expected two GUI outcomes and optional headless outcome; no diagnostic selected.", file=sys.stderr)
        return 0
    if any(value not in OUTCOMES for value in sys.argv[1:]):
        print("Unrecognized gate outcome; no diagnostic selected.", file=sys.stderr)
    selected = select_log(*sys.argv[1:])
    if selected:
        print(selected)
    return 0


if __name__ == "__main__":
    sys.exit(main())
