#!/usr/bin/env python3
"""Contract: every APVTS state copy/replacement flushes under its tree lock.

This gate examines the four production copy/replace sites, including host-state
restore and user-preset recall. It deliberately fails if a new site is added
without updating the audited count and its lock/sync ordering.
"""

from pathlib import Path
import re
import unittest


SOURCE = Path(__file__).resolve().parents[1] / "src/plugin/PluginProcessor.cpp"


def audit_sites(code):
    """Return (operation, line, reason) for each unguarded state operation."""
    failures = []
    for site in re.finditer(r"\bapvts\.(copyState|replaceState)\s*\(", code):
        preceding = "\n".join(code[:site.start()].splitlines()[-4:])
        lock = preceding.rfind("treeLock(apvts.treeLock())")
        sync = preceding.rfind("apvts.syncNow()")
        if lock < 0 or sync <= lock:
            failures.append((site.group(1), code.count("\n", 0, site.start()) + 1,
                             "missing or out-of-order tree lock / syncNow"))
    return failures


class APVTSStateLockContract(unittest.TestCase):
    def test_all_copy_and_replace_sites_lock_then_sync(self):
        code = SOURCE.read_text(encoding="utf-8")
        sites = list(re.finditer(r"\bapvts\.(copyState|replaceState)\s*\(", code))
        self.assertEqual(len(sites), 4, "review every added APVTS state copy/replacement site")
        self.assertEqual(audit_sites(code), [], "all APVTS operations must lock, then sync, before state access")

    def test_in_memory_negative_oracles(self):
        # Mutation-check the gate with synthetic text only: production never
        # temporarily removes the mandatory synchronization call.
        correctly_guarded = """\
const juce::ScopedLock treeLock(apvts.treeLock());
apvts.syncNow();
apvts.replaceState(restored);
"""
        missing_sync = """\
const juce::ScopedLock treeLock(apvts.treeLock());
apvts.replaceState(restored);
"""
        reversed_order = """\
apvts.syncNow();
const juce::ScopedLock treeLock(apvts.treeLock());
apvts.copyState();
"""
        self.assertEqual(audit_sites(correctly_guarded), [], "proper locking is accepted")
        self.assertEqual(len(audit_sites(missing_sync)), 1, "missing sync is detected")
        self.assertEqual(len(audit_sites(reversed_order)), 1, "sync outside the lock is detected")


if __name__ == "__main__":
    unittest.main()
