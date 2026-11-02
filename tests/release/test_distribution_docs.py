#!/usr/bin/env python3
"""Synthetic distribution-payload tests; these never qualify a plugin binary."""
from __future__ import annotations

import hashlib
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

ROOT = Path(__file__).resolve().parents[2]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


PACKAGER = load_module("cs80_package_ci_artifacts_test", ROOT / "scripts/package-ci-artifacts.py")
REQUIRED = {
    "LICENSE": b"synthetic project licence fixture\n",
    "licenses/JUCE-LICENSE.md": b"synthetic JUCE licence fixture\n",
    "licenses/MTS-ESP-LICENSE.txt": b"synthetic MTS notice fixture\n",
    "licenses/BarlowCondensed-OFL.txt": b"synthetic OFL notice fixture\n",
    "licenses/BarlowCondensed-SOURCE.txt": b"synthetic font attribution fixture\n",
    "INSTALL_AND_QUICKSTART.md": b"synthetic installation fixture\n",
}
INPUTS = {
    "LICENSE": "LICENSE",
    "licenses/MTS-ESP-LICENSE.txt": "deps/mts-esp/LICENSE",
    "licenses/BarlowCondensed-OFL.txt": "assets/fonts/barlow-condensed/OFL.txt",
    "licenses/BarlowCondensed-SOURCE.txt": "assets/fonts/barlow-condensed/SOURCE.txt",
    "INSTALL_AND_QUICKSTART.md": "docs/INSTALL_AND_QUICKSTART.md",
}


class DistributionDocsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cs80-distribution-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        for output, source in INPUTS.items():
            path = self.root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(REQUIRED[output])
        (self.root / "docs/CI_DISTRIBUTION_STATUS.md").write_text("synthetic status fixture\n")
        (self.root / "CMakeLists.txt").write_text("project(CS80Reference VERSION 1.0.0 LANGUAGES C CXX)\n")
        (self.root / ".gitignore").write_text("build/\npackages/\n")
        for relative in ("installer/linux/make-deb.sh", "scripts/distribution_docs.py"):
            original = ROOT / relative
            if original.is_file():
                dest = self.root / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, dest)
        self.git("init", "--quiet")
        self.git("config", "user.name", "CS80 synthetic fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "synthetic distribution fixture")
        self.commit = self.git("rev-parse", "HEAD").strip()
        self.build = self.root / "build"
        self.license = self.build / "_deps/juce-src/LICENSE.md"
        self.license.parent.mkdir(parents=True)
        self.license.write_bytes(REQUIRED["licenses/JUCE-LICENSE.md"])
        self.report = self.build / "ci-juce-provenance.txt"
        self.report.write_text(
            f"verification=PASS\nsource_clean=true\nactual_sha={PACKAGER.JUCE_SHA}\n"
            f"actual_version=8.0.11\nworkflow_commit={self.commit}\n"
            f"configured_source={self.license.parent}\n")
        self.env = mock.patch.dict(os.environ, {"GITHUB_SHA": self.commit})
        self.env.start()
        self.addCleanup(self.env.stop)

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True)

    def context(self):
        with mock.patch.object(PACKAGER, "ROOT", self.root):
            return PACKAGER.read_ci_context(self.build)

    def helper(self):
        return load_module("cs80_distribution_docs_test", ROOT / "scripts/distribution_docs.py")

    def test_ci_payload_preserves_every_notice(self):
        commit, metadata, payload = self.context()
        self.assertEqual(commit, self.commit)
        for name, data in REQUIRED.items():
            with self.subTest(name=name):
                self.assertIn(name, payload)
                self.assertEqual(payload[name], data)
        self.assertIn(self.commit.encode(), payload["SOURCE.txt"])

    def test_ci_payload_missing_notice_fails_closed(self):
        path = self.root / "assets/fonts/barlow-condensed/OFL.txt"
        path.unlink()
        self.git("add", "-u")
        self.git("commit", "--quiet", "-m", "remove notice fixture")
        self.commit = self.git("rev-parse", "HEAD").strip()
        self.report.write_text(self.report.read_text().replace(os.environ["GITHUB_SHA"], self.commit))
        with mock.patch.dict(os.environ, {"GITHUB_SHA": self.commit}):
            with self.assertRaises((ValueError, OSError)):
                self.context()

    def test_staged_metadata_is_exact_and_explicitly_limited(self):
        out = self.build / "distribution-docs"
        manifest = self.helper().stage_documents(self.root, self.license, out)
        self.assertEqual(manifest["source_commit"], self.commit)
        self.assertIn("not", manifest["qualification"].lower())
        self.assertIn("checkout", manifest["scope"].lower())
        serialized = json.loads((out / "distribution-manifest.json").read_text())
        for item in serialized["files"]:
            self.assertTrue((out / item["path"]).is_file(), item["path"])
            data = (out / item["path"]).read_bytes()
            self.assertEqual(item["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(serialized, manifest)
        for name, data in REQUIRED.items():
            self.assertTrue((out / name).is_file(), name)
            self.assertEqual((out / name).read_bytes(), data)
        actual = {p.relative_to(out).as_posix() for p in out.rglob("*") if p.is_file()}
        expected = {f["path"] for f in manifest["files"]} | {"distribution-manifest.json"}
        self.assertEqual(actual, expected)
        for item in manifest["files"]:
            data = (out / item["path"]).read_bytes()
            self.assertEqual(item["size"], len(data))
            self.assertEqual(item["sha256"], hashlib.sha256(data).hexdigest())

    def test_stage_rejects_missing_notice(self):
        for source in INPUTS.values():
            with self.subTest(source=source):
                original = self.root / source
                saved = original.read_bytes()
                original.unlink()
                try:
                    with self.assertRaises((ValueError, OSError)):
                        self.helper().notice_payload(self.root, self.license)
                finally:
                    original.write_bytes(saved)

    @unittest.skipIf(os.name == "nt", "symlink fixture requires Unix; Windows packaging still rejects links")
    def test_stage_rejects_linked_notice(self):
        path = self.root / "assets/fonts/barlow-condensed/OFL.txt"
        original = path.read_bytes()
        path.unlink()
        target = self.build / "linked-notice.txt"
        target.write_bytes(original)
        path.symlink_to(target)
        with self.assertRaises(ValueError):
            self.helper().notice_payload(self.root, self.license)

    def test_ci_and_stage_reject_untracked_notice_replacement(self):
        path = self.root / "assets/fonts/barlow-condensed/OFL.txt"
        data = path.read_bytes()
        self.git("rm", "--", "assets/fonts/barlow-condensed/OFL.txt")
        self.git("commit", "--quiet", "-m", "untracked notice fixture")
        new_commit = self.git("rev-parse", "HEAD").strip()
        path.write_bytes(data)
        self.report.write_text(self.report.read_text().replace(self.commit, new_commit))
        with mock.patch.dict(os.environ, {"GITHUB_SHA": new_commit}):
            with self.assertRaises((ValueError, OSError, subprocess.SubprocessError)):
                self.context()
            with self.assertRaises((ValueError, OSError, subprocess.SubprocessError)):
                self.helper().stage_documents(self.root, self.license, self.build / "out")

    def test_windows_installer_requires_generated_documents(self):
        installer = (ROOT / "installer/windows/cs80-reference.iss").read_text()
        self.assertIn("#ifndef DistributionDocs", installer)
        self.assertIn("#error DistributionDocs", installer)
        self.assertIn('Source: "{#DistributionDocs}\\*"', installer)
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertIn('python scripts/distribution_docs.py --root . --juce-license "$license"', workflow)
        self.assertIn('"/DDistributionDocs=$distributionDocs"', workflow)

    def test_stage_rejects_dirty_source(self):
        (self.root / "LICENSE").write_bytes(b"modified fixture\n")
        with self.assertRaises(ValueError):
            self.helper().stage_documents(self.root, self.license, self.build / "out")

    def test_stage_rejects_workflow_identity_mismatch(self):
        with mock.patch.dict(os.environ, {"GITHUB_SHA": "f" * 40}):
            with self.assertRaises(ValueError):
                self.helper().stage_documents(self.root, self.license, self.build / "out")

    def test_stage_rejects_existing_output(self):
        out = self.build / "out"
        out.mkdir()
        (out / "keep").write_bytes(b"keep")
        with self.assertRaises((ValueError, OSError)):
            self.helper().stage_documents(self.root, self.license, out)
        self.assertEqual((out / "keep").read_bytes(), b"keep")

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("dpkg-deb"), "native Linux package fixture")
    def test_deb_payload_roundtrip(self):
        artifacts = self.build / "CS80Reference_artefacts/Release"
        for relative in ("VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so", "Standalone/CS-80 Reference"):
            binary = artifacts / relative
            binary.parent.mkdir(parents=True, exist_ok=True)
            binary.write_bytes(b"synthetic fixture, not a loadable plugin\n")
            binary.chmod(0o755)
        subprocess.run(["bash", str(self.root / "installer/linux/make-deb.sh"), str(self.build), str(self.root / "packages")], check=True, stdout=subprocess.PIPE, text=True)
        archive, = (self.root / "packages").glob("*.deb")
        extracted = Path(self.temp.name) / "extracted"
        subprocess.run(["dpkg-deb", "--extract", str(archive), str(extracted)], check=True)
        docs = extracted / "usr/share/doc/cs80-reference"
        for name, data in REQUIRED.items():
            with self.subTest(name=name):
                self.assertTrue((docs / name).is_file(), name)
                self.assertEqual((docs / name).read_bytes(), data)
        self.assertIn(self.commit, (docs / "SOURCE.txt").read_text())
        self.assertTrue((docs / "distribution-manifest.json").is_file())
        for item in json.loads((docs / "distribution-manifest.json").read_text())["files"]:
            self.assertTrue((docs / item["path"]).is_file(), item["path"])
            data = (docs / item["path"]).read_bytes()
            self.assertEqual(item["size"], len(data))
            self.assertEqual(item["sha256"], hashlib.sha256(data).hexdigest())


if __name__ == "__main__":
    unittest.main(verbosity=2)
