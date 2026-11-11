#!/usr/bin/env python3
"""Fail-closed generator checks; no compiler or original-checkout mutation."""
import hashlib
import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
MODULE = Path(sys.argv.pop()).resolve()
SPEC = importlib.util.spec_from_file_location("bridge", Path(__file__).resolve().parents[1] / "scripts/patch-juce-state-mirror.py")
bridge = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bridge)

class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        for name in bridge.PINS:
            target = self.source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(MODULE / name, target)
        self.original = {name: (self.source / name).read_bytes() for name in bridge.PINS}
    def tearDown(self):
        self.temp.cleanup()
    def test_cpp_and_objective_cpp_and_idempotence(self):
        cpp, mm = self.root / "out/module.cpp", self.root / "out/module.mm"
        bridge.generate(self.source / "juce_audio_processors.cpp", cpp)
        bridge.generate(self.source / "juce_audio_processors.mm", mm)
        self.assertEqual(cpp.read_text(), mm.read_text())
        self.assertEqual(cpp.read_text().count('cs80_AudioProcessorValueTreeState.cpp"'), 1)
        patched = (cpp.parent / "cs80_AudioProcessorValueTreeState.cpp").read_text()
        self.assertEqual(patched.count("ParameterStateMirrorScope::suppresses"), 1)
        self.assertEqual(patched.count('#include "plugin/ParameterStateMirror.h"'), 1)
        before = cpp.stat().st_mtime_ns
        bridge.generate(self.source / "juce_audio_processors.cpp", cpp)
        self.assertEqual(cpp.stat().st_mtime_ns, before)
        for name, data in self.original.items():
            self.assertEqual((self.source / name).read_bytes(), data)
    def test_each_modified_source_rejected_before_output(self):
        for name in bridge.PINS:
            with self.subTest(name=name):
                path = self.source / name
                path.write_bytes(self.original[name] + b"\n// changed input\n")
                output = self.root / "rejected/module.cpp"
                with self.assertRaisesRegex(ValueError, "unrecognized JUCE source"):
                    bridge.generate(self.source / "juce_audio_processors.cpp", output)
                self.assertFalse(output.exists())
                self.assertFalse((output.parent / "cs80_AudioProcessorValueTreeState.cpp").exists())
                path.write_bytes(self.original[name])
    def test_pristine_destination_rejected(self):
        for output in (self.source / "juce_audio_processors.cpp", self.source / "generated/module.cpp"):
            with self.assertRaisesRegex(ValueError, "pristine module"):
                bridge.generate(self.source / "juce_audio_processors.cpp", output)
        for name, data in self.original.items():
            self.assertEqual((self.source / name).read_bytes(), data)
    def test_language_and_unknown_source_rejected(self):
        with self.assertRaises(ValueError):
            bridge.generate(self.source / "juce_audio_processors.cpp", self.root / "bad.mm")
        with self.assertRaises(ValueError):
            bridge.generate(self.source / "unexpected.cpp", self.root / "bad.cpp")
    def test_anchor_cardinality(self):
        for text in ("absent", bridge.ANCHOR + bridge.ANCHOR):
            with self.assertRaises(ValueError):
                bridge.replace_once(text, bridge.ANCHOR, bridge.REPLACEMENT)
    def test_crlf_checkout_supported(self):
        for name in bridge.PINS:
            path = self.source / name
            path.write_bytes(path.read_text().replace("\n", "\r\n").encode())
        bridge.generate(self.source / "juce_audio_processors.cpp", self.root / "crlf/module.cpp")
        self.assertTrue((self.root / "crlf/module.cpp").exists())

unittest.main()
