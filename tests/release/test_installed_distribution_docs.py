#!/usr/bin/env python3
"""Pre-registered independent extraction checks; synthetic documents, not audio."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import sys
import unittest

import test_distribution_docs as staging_tests

ROOT = staging_tests.ROOT
load_module = staging_tests.load_module


class InstalledDistributionDocsTests(unittest.TestCase):
    setUp = staging_tests.DistributionDocsTests.setUp
    git = staging_tests.DistributionDocsTests.git
    helper = staging_tests.DistributionDocsTests.helper

    def prepare(self):
        self.out = self.build / "extracted-document-payload"
        self.helper().stage_documents(self.root, self.license, self.out)
        self.verifier = load_module("cs80_installed_distribution_verifier",
                                    ROOT / "scripts/verify_distribution_docs.py")

    def verify(self):
        return self.verifier.verify_documents(self.root, self.license, self.out, self.commit)

    def change_manifest(self, callback):
        path = self.out / "distribution-manifest.json"
        manifest = json.loads(path.read_text())
        callback(manifest)
        path.write_text(json.dumps(manifest))

    def test_exact_extracted_payload(self):
        self.prepare()
        result = self.verify()
        self.assertEqual(result["source_commit"], self.commit)
        self.assertEqual(result["document_count"], 7)
        self.assertIn("not", result["qualification"].lower())

    def test_missing_required_document(self):
        self.prepare()
        (self.out / "licenses/BarlowCondensed-OFL.txt").unlink()
        with self.assertRaisesRegex(ValueError, "document|payload"):
            self.verify()

    def test_rehashed_counterfeit_is_not_a_source_notice(self):
        self.prepare()
        path = self.out / "licenses/MTS-ESP-LICENSE.txt"
        path.write_bytes(b"counterfeit licence with self-consistent manifest\n")
        def edit(manifest):
            row = next(row for row in manifest["files"] if row["path"] == "licenses/MTS-ESP-LICENSE.txt")
            row.update(size=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        self.change_manifest(edit)
        with self.assertRaisesRegex(ValueError, "source|notice"):
            self.verify()

    def test_wrong_source_identity(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest.update(source_commit="f" * 40))
        with self.assertRaisesRegex(ValueError, "source|commit"):
            self.verify()

    def test_wrong_source_url(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest.update(source_url="https://example.invalid/other-source"))
        with self.assertRaisesRegex(ValueError, "source|URL"):
            self.verify()

    def test_manifest_cannot_claim_release_acceptance(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest.update(qualification="Release accepted"))
        with self.assertRaisesRegex(ValueError, "scope|qualification"):
            self.verify()

    def test_wrong_source_tree(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest.update(source_tree="f" * 40))
        with self.assertRaisesRegex(ValueError, "tree|source"):
            self.verify()

    def test_wrong_expected_checkout(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "source|commit|checkout"):
            self.verifier.verify_documents(self.root, self.license, self.out, "f" * 40)

    def test_duplicate_manifest_row(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest["files"].append(manifest["files"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate|inventory"):
            self.verify()

    def test_missing_manifest_row(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest["files"].pop())
        with self.assertRaisesRegex(ValueError, "inventory"):
            self.verify()

    def test_manifest_digest_mismatch(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest["files"][0].update(sha256="0" * 64))
        with self.assertRaisesRegex(ValueError, "digest|hash"):
            self.verify()

    def test_unsafe_manifest_path(self):
        self.prepare()
        self.change_manifest(lambda manifest: manifest["files"][0].update(path="../outside"))
        with self.assertRaisesRegex(ValueError, "inventory|path"):
            self.verify()

    def test_rehashed_source_text_cannot_claim_qualification(self):
        self.prepare()
        path = self.out / "SOURCE.txt"
        path.write_text("Qualified source: " + self.commit + "\n")
        def edit(manifest):
            row = next(row for row in manifest["files"] if row["path"] == "SOURCE.txt")
            row.update(size=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        self.change_manifest(edit)
        with self.assertRaisesRegex(ValueError, "source|SOURCE|scope"):
            self.verify()

    def test_unexpected_document(self):
        self.prepare()
        (self.out / "release-certificate.txt").write_text("Not a genuine acceptance receipt\n")
        with self.assertRaisesRegex(ValueError, "unexpected|inventory"):
            self.verify()

    def test_debian_copyright_copy_is_exact(self):
        self.prepare()
        (self.out / "copyright").write_bytes((self.out / "LICENSE").read_bytes())
        self.verify()
        (self.out / "copyright").write_text("wrong copyright notice\n")
        with self.assertRaisesRegex(ValueError, "copyright|notice"):
            self.verify()

    @unittest.skipIf(os.name == "nt", "requires symlink creation; native Windows receipt is separate")
    def test_linked_document_parent_rejected(self):
        self.prepare()
        directory = self.out / "licenses"
        relocated = self.build / "relocated-licences"
        directory.rename(relocated)
        directory.symlink_to(relocated, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "link|payload"):
            self.verify()


if __name__ == "__main__":
    unittest.main(verbosity=2)
