#!/usr/bin/env python3
"""Offline regressions for the CI JUCE source gate and release wiring.

The temporary checkout is inert and is not JUCE. No source is fetched, built,
signed, installed or executed; only the actual CMake verifier and Git run.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "scripts/verify-ci-juce-source.cmake"
VERSION = "8.0.11"
WORKFLOW_SHA = "1234567890abcdef1234567890abcdef12345678"
CMAKE = "cmake"


class SourceGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cs80 source evidence space ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build space"
        self.source = self.build / "_deps/juce-src"
        self.source.mkdir(parents=True)
        self.env = dict(os.environ, GITHUB_SHA=WORKFLOW_SHA, GITHUB_RUN_ID="42",
                        GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_TERMINAL_PROMPT="0")
        self.git("init", "--quiet", "--template=")
        self.git("config", "user.name", "Synthetic fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("remote", "add", "origin", "https://example.invalid/inert-fixture.git")
        (self.source / "CMakeLists.txt").write_text(f"project(JUCE VERSION {VERSION})\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Inert verifier fixture, not JUCE")
        self.sha = self.git("rev-parse", "HEAD").strip()
        self.cache()

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.source), *args], env=self.env,
                              check=True, capture_output=True, text=True, timeout=15).stdout

    def cache(self, source=None, override=""):
        # Forward slashes are accepted by CMake on Windows too. No configure or
        # code execution from this inert CMakeLists.txt is needed for load_cache.
        (self.build / "CMakeCache.txt").write_text(
            f"JUCE_SOURCE_DIR:STATIC={(source or self.source).as_posix()}\n"
            f"FETCHCONTENT_SOURCE_DIR_JUCE:PATH={override}\n"
            f"CMAKE_HOME_DIRECTORY:INTERNAL={self.root.as_posix()}\n")

    def verify(self, failure=None, *, sha=None, version=VERSION):
        result = subprocess.run(
            [CMAKE, f"-DBUILD_DIR={self.build}", f"-DEXPECTED_JUCE_SHA={sha or self.sha}",
             f"-DEXPECTED_JUCE_VERSION={version}", "-P", str(VERIFIER)],
            env=self.env, capture_output=True, text=True, timeout=15)
        report = (self.build / "ci-juce-provenance.txt").read_text()
        if failure is None:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("verification=PASS\n", report)
            self.assertNotIn("verification=FAIL", report)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("verification=FAIL\n", report)
            self.assertNotIn("verification=PASS", report)
            self.assertIn(failure, report)
        return report

    def test_clean_exact_checkout_records_identity(self):
        report = self.verify()
        for field in (f"actual_sha={self.sha}", f"actual_version={VERSION}",
                      "source_clean=true", f"workflow_commit={WORKFLOW_SHA}", "workflow_run=42"):
            self.assertIn(field + "\n", report)

    def test_wrong_commit(self):
        self.verify("does not match expected", sha="0" * 40)

    def test_wrong_version(self):
        self.verify("does not match expected", version="8.0.10")

    def test_tracked_modification(self):
        with (self.source / "CMakeLists.txt").open("a") as stream:
            stream.write("# Modified fixture\n")
        self.verify("The configured JUCE checkout is dirty")

    def test_untracked_source(self):
        (self.source / "unexpected-source.cpp").write_text("// Inert untracked fixture\n")
        self.verify("The configured JUCE checkout is dirty")

    def test_explicit_override(self):
        self.cache(override=self.source.as_posix())
        self.verify("FETCHCONTENT_SOURCE_DIR_JUCE overrides are not allowed")

    def test_local_source_path(self):
        local = self.root / "deps/juce"
        local.mkdir(parents=True)
        self.cache(source=local)
        self.verify("CI must build the fetched _deps/juce-src checkout")

    def test_missing_git_metadata(self):
        (self.source / ".git").rename(self.root / "detached-git-metadata")
        self.verify("has no .git metadata")

    def test_missing_configured_source(self):
        self.cache(source=self.root / "absent")
        self.verify("Configured JUCE_SOURCE_DIR is missing")

    def test_failed_recheck_replaces_previous_pass(self):
        self.verify()
        (self.build / "CMakeCache.txt").unlink()
        self.verify("CMakeCache.txt is missing")


class ReleaseWorkflowTests(unittest.TestCase):
    def test_every_release_job_checks_before_build_and_retains_evidence(self):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        # Inspect this checked-in workflow's simple job/step mapping without
        # adding a YAML parser dependency to the project's Python-only tests.
        jobs = re.split(r"^  ([\w-]+):\n", workflow.split("\njobs:\n", 1)[1], flags=re.M)
        jobs = dict(zip(jobs[1::2], jobs[2::2]))
        self.assertEqual(set(jobs), {"macos-universal", "windows-installer", "linux-packages"})
        pin = re.search(r"GIT_TAG ([0-9a-f]{40})", (ROOT / "CMakeLists.txt").read_text()).group(1)
        for name, job in jobs.items():
            with self.subTest(job=name):
                steps = re.split(r"^      - ", job, flags=re.M)[1:]
                verifiers = [step for step in steps if "-P scripts/verify-ci-juce-source.cmake" in step]
                self.assertEqual(len(verifiers), 1, f"{name} must verify JUCE exactly once")
                verifier = verifiers[0]
                self.assertIn("-DBUILD_DIR=build", verifier)
                self.assertIn(f"-DEXPECTED_JUCE_SHA={pin}", verifier)
                self.assertIn(f"-DEXPECTED_JUCE_VERSION={VERSION}", verifier)
                self.assertNotRegex(verifier, r"(?m)^        (?:if|continue-on-error):")
                self.assertLess(job.index("name: Configure"), job.index(verifier))
                self.assertLess(job.index(verifier), job.index("name: Build\n"))
                evidence = [step for step in steps if "path: build/ci-juce-provenance.txt" in step]
                self.assertEqual(len(evidence), 1, f"{name} must retain source-gate evidence")
                self.assertIn("if: always()", evidence[0])
                self.assertIn("uses: actions/upload-artifact@", evidence[0])
                self.assertIn(f"name: cs80-{name}-juce-evidence", evidence[0])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake", help="CMake executable used by CTest")
    args, remaining = parser.parse_known_args()
    CMAKE = args.cmake
    unittest.main(argv=[__file__, *remaining], verbosity=2)
