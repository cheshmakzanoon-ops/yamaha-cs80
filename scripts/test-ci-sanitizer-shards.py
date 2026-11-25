#!/usr/bin/env python3
"""Bounded partition/evidence controls; not registered in the product CTest roster."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).with_name("ci-sanitizer-shards.py")
spec = importlib.util.spec_from_file_location("shards", SCRIPT)
shards = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shards)


class ShardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="cs80-sanitizer-shard-tests-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = Path(cls.temporary.name)
        cls.environment = mock.patch.dict(os.environ, {
            "GITHUB_SHA": "a" * 40, "GITHUB_RUN_ID": "1", "GITHUB_RUN_ATTEMPT": "1",
            **shards.SANITIZER_ENV})
        cls.environment.start()
        cls.addClassCleanup(cls.environment.stop)
        cmake = os.environ.get("CMAKE_COMMAND", "cmake")
        cls.ctest = os.environ.get("CTEST_COMMAND", "ctest")
        source = cls.root / "fixture"
        source.mkdir()
        (source / "skip.py").write_text("raise SystemExit(77)\n")
        lines = ["cmake_minimum_required(VERSION 3.22)", "project(ShardFixture NONE)",
                 "enable_testing()"]
        for index in range(1, shards.TEST_COUNT + 1):
            name = f"fixture_{index:03}"
            command = f'"{sys.executable}" "{source / "skip.py"}"' if index == 80 else '"${CMAKE_COMMAND}" -E true'
            lines += [f'add_test(NAME {name} COMMAND {command})',
                      f'set_tests_properties({name} PROPERTIES RUN_SERIAL TRUE TIMEOUT 10)']
        lines.append('set_tests_properties(fixture_080 PROPERTIES SKIP_RETURN_CODE 77)')
        # Exercise the real CTest label footer in every shard. The earlier
        # unlabeled fixture missed the packaging totals emitted in native CI.
        lines.append('set_tests_properties(fixture_001 fixture_002 fixture_003 '
                     'PROPERTIES LABELS "packaging")')
        lines.append('set_tests_properties(fixture_004 fixture_005 fixture_006 '
                     'PROPERTIES LABELS "artifact checks;packaging")')
        (source / "CMakeLists.txt").write_text("\n".join(lines) + "\n")
        cls.build = cls.root / "build"
        flags = [f"-D{key}={value}" for key, value in shards.BUILD_FLAGS.items()]
        subprocess.run([cmake, "-S", str(source), "-B", str(cls.build), *flags],
                       check=True, capture_output=True, text=True)
        cls.artifacts = cls.root / "originals"
        for index in range(1, 4):
            output = cls.artifacts / f"shard-{index}"
            for operation, extra in (("plan", ["--shard", str(index)]), ("run", [])):
                result = subprocess.run([sys.executable, str(SCRIPT), operation,
                    "--ctest", cls.ctest, "--build", str(cls.build), "--output", str(output), *extra],
                    capture_output=True, text=True, timeout=30)
                if result.returncode:
                    raise RuntimeError(result.stdout + result.stderr)

    def setUp(self):
        self.case = self.root / self._testMethodName
        shutil.copytree(self.artifacts, self.case)

    def verify(self, success=True, jobs="success"):
        result = subprocess.run([sys.executable, str(SCRIPT), "verify", "--input", str(self.case),
            "--output", str(self.case / "summary.json"), "--jobs-result", jobs],
            capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0 if success else 1, result.stdout + result.stderr)
        self.assertEqual(shards.read(self.case / "summary.json")["accepted"], success)

    def change(self, relative, function):
        path = self.case / relative
        value = shards.read(path)
        function(value)
        shards.write(path, value)

    def test_three_original_serial_shards_cover_full_roster(self):
        self.verify()
        summary = shards.read(self.case / "summary.json")
        self.assertEqual((summary["tests"], summary["passed"], summary["skipped"]),
                         (232, 231, ["fixture_080"]))
        plans = [shards.read(self.case / f"shard-{index}/plan.json") for index in (1, 2, 3)]
        self.assertEqual([len(plans[0]["partitions"][i]) for i in range(3)], [78, 77, 77])

    def test_missing_shard_fails(self):
        shutil.rmtree(self.case / "shard-2")
        self.verify(False)

    def test_actual_ctest_label_footers_are_retained(self):
        self.verify()
        for index in (1, 2, 3):
            log = (self.case / f"shard-{index}/LastTest.log").read_text()
            tail = log.split("End testing:", 1)[1]
            self.assertIn("artifact checks", tail)
            self.assertIn("packaging", tail)
            self.assertEqual(tail.count("sec*proc"), 2)

    def test_malformed_or_foreign_footer_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/LastTest.log"
        original = path.read_text()
        label_line = next(line for line in original.splitlines() if line.startswith("packaging"))
        for replacement in ("unregistered =   0.01 sec*proc", "packaging = nan sec*proc",
                            "packaging = -1.00 sec*proc", "packaging = 1 sec", ""):
            with self.subTest(replacement=replacement):
                path.write_text(original.replace(label_line, replacement))
                self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
                self.verify(False)

    def test_appended_output_or_duplicate_footer_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/LastTest.log"
        original = path.read_text()
        label_line = next(line for line in original.splitlines() if line.startswith("packaging"))
        for extra in ("unexpected trailing output\n", "End testing: duplicate\n", label_line + "\n"):
            with self.subTest(extra=extra):
                path.write_text(original + extra)
                self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
                self.verify(False)

    def test_duplicate_shard_fails(self):
        shutil.rmtree(self.case / "shard-2")
        shutil.copytree(self.case / "shard-1", self.case / "shard-2")
        self.verify(False)

    def test_changed_partition_fails(self):
        self.change("shard-1/plan.json", lambda p: p["partitions"][0].pop())
        self.verify(False)

    def test_failed_or_cancelled_jobs_fail(self):
        for status in ("failure", "cancelled", "skipped"):
            self.verify(False, status)

    def test_failed_signaled_or_incomplete_original_ctest_fails(self):
        receipt = shards.read(self.case / "shard-1/result.json")
        for status, completed in ((8, True), (-15, True), (None, False)):
            value = dict(receipt, ctest_returncode=status, completed=completed)
            shards.write(self.case / "shard-1/result.json", value)
            self.verify(False)

    def test_changed_run_identity_fails(self):
        self.change("shard-1/result.json", lambda r: r["identity"].update(GITHUB_RUN_ATTEMPT="2"))
        self.verify(False)

    def test_missing_junit_or_last_test_fails(self):
        for filename in ("ctest-results.xml", "LastTest.log"):
            path = self.case / "shard-1" / filename
            original = path.read_bytes()
            path.unlink()
            self.verify(False)
            path.write_bytes(original)

    def test_changed_runtime_flags_fail(self):
        self.change("shard-1/result.json", lambda r: r["environment"].update(ASAN_OPTIONS="detect_leaks=0"))
        self.verify(False)

    def test_changed_build_scale_fails(self):
        path = self.case / "shard-1/CMakeCache.txt"
        path.write_text(path.read_text().replace("CS80_TEST_TIMEOUT_SCALE:UNINITIALIZED=4",
                                                "CS80_TEST_TIMEOUT_SCALE:UNINITIALIZED=5"))
        self.verify(False)

    def test_undeclared_skip_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/ctest-results.xml"
        tree = ET.parse(path)
        case = next(tree.getroot().iter("testcase"))
        ET.SubElement(case, "skipped")
        tree.write(path)
        self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
        self.verify(False)

    def test_invalid_junit_status_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/ctest-results.xml"
        original = path.read_bytes()
        for status in ("notrun", "disabled", "unknown"):
            path.write_bytes(original)
            tree = ET.parse(path)
            next(tree.getroot().iter("testcase")).set("status", status)
            tree.write(path)
            self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
            self.verify(False)

    def test_nested_junit_cases_fail_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/ctest-results.xml"
        tree = ET.parse(path)
        root = tree.getroot()
        children = list(root)
        wrapper = ET.SubElement(root, "unknown-wrapper")
        for case in children:
            root.remove(case)
            wrapper.append(case)
        tree.write(path)
        self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
        self.verify(False)

    def test_nested_failure_or_skip_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/ctest-results.xml"
        original = path.read_bytes()
        for marker in ("failure", "error", "skipped"):
            path.write_bytes(original)
            tree = ET.parse(path)
            wrapper = ET.SubElement(next(tree.getroot().iter("testcase")), "unknown-child")
            ET.SubElement(wrapper, marker)
            tree.write(path)
            self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
            self.verify(False)

    def test_partial_last_test_with_footer_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/LastTest.log"
        path.write_text(path.read_text().replace('"fixture_001" end time:', '"wrong_test" end time:', 1))
        self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
        self.verify(False)

    def test_duplicate_session_or_missing_output_frame_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/LastTest.log"
        original = path.read_text()
        prefix = "\n".join(original.splitlines()[:2]) + "\n"
        for log in (prefix + original, original.replace("Output:\n", "", 1)):
            path.write_text(log)
            self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
            self.verify(False)

    def test_duplicate_or_omitted_junit_fails_even_with_rehashed_receipt(self):
        path = self.case / "shard-1/ctest-results.xml"
        original = path.read_bytes()
        for duplicate in (False, True):
            path.write_bytes(original)
            tree = ET.parse(path)
            root = tree.getroot()
            case = next(root.iter("testcase"))
            if duplicate:
                root.append(copy.deepcopy(case))
            else:
                root.remove(case)
            tree.write(path)
            self.change("shard-1/result.json", lambda r: r["sha256"].update({path.name: shards.digest(path)}))
            self.verify(False)

    def test_duplicate_roster_disabled_fixture_and_dependency_fail(self):
        roster = shards.read(self.case / "shard-1/roster.json")
        for key, value in (("DISABLED", True), ("FIXTURES_SETUP", ["state"]),
                           ("FIXTURES_REQUIRED", ["state"]), ("FIXTURES_CLEANUP", ["state"]),
                           ("DEPENDS", ["fixture_002"]), ("RUN_SERIAL", False)):
            mutated = copy.deepcopy(roster)
            mutated["tests"][0]["properties"] = [p for p in mutated["tests"][0]["properties"] if p["name"] != key]
            mutated["tests"][0]["properties"].append({"name": key, "value": value})
            with self.assertRaises(ValueError):
                shards.contract(mutated)
        roster["tests"][0]["name"] = roster["tests"][1]["name"]
        with self.assertRaises(ValueError):
            shards.contract(roster)

    def test_out_of_range_shard_fails(self):
        for index in (0, 4, -1):
            result = subprocess.run([sys.executable, str(SCRIPT), "plan", "--shard", str(index),
                "--build", str(self.build), "--output", str(self.case / "bad-plan")],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertFalse((self.case / "bad-plan").exists())

    def test_original_attempt_cannot_be_replaced(self):
        result = subprocess.run([sys.executable, str(SCRIPT), "run", "--ctest", self.ctest,
            "--build", str(self.build), "--output", str(self.case / "shard-1")],
            capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Refusing to replace an original attempt", result.stderr)


if __name__ == "__main__":
    unittest.main()
