"""Synthetic verifier controls only; these are never real-host evidence."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("reaper_validator", ROOT / "scripts/verify-reaper-smoke.py")
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class AudioControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads((ROOT / "tests/hosts/reaper/contract-v1.json").read_text())
        samples = [0.0] * (96000 * 2)
        for frame in range(12000, 72000):
            v = 0.1 * math.sin(2 * math.pi * 440 * frame / 48000)
            samples[2 * frame:2 * frame + 2] = [v, v]
        cls.waves = {name: (48000, 2, samples.copy()) for name in cls.contract["render"]["files"]}
        cls.waves["no-midi"] = (48000, 2, [0.0] * len(samples))
        automated = samples.copy()
        automated[43200 * 2:57600 * 2] = [0.0] * ((57600 - 43200) * 2)
        cls.waves["automated"] = (48000, 2, automated)

    def test_control_is_accepted(self):
        result = validator.verify_audio(self.contract, self.waves)
        self.assertGreater(result["baseline_active_rms"], 0.001)

    def test_each_counterfeit_is_rejected(self):
        mutations = {
            "wrong-rate": lambda w: w.update(baseline=(44100, 2, w["baseline"][2])),
            "wrong-channels": lambda w: w.update(baseline=(48000, 1, w["baseline"][2])),
            "missing-file": lambda w: w.pop("restored"),
            "truncated-frames": lambda w: w.update(baseline=(48000, 2, w["baseline"][2][:-2])),
            "silent-baseline": lambda w: w.update(baseline=w["no-midi"]),
            "missing-midi-counterfactual": lambda w: w.update({"no-midi": w["baseline"]}),
            "lost-automation": lambda w: w.update(automated=w["baseline"]),
            "wrong-restored-state": lambda w: w["restored"][2].__setitem__(50000, 0.9),
            "early-automation-change": lambda w: w["automated"][2].__setitem__(50000, 0.9),
            "pre-note-audio": lambda w: w["baseline"][2].__setitem__(500, 0.1),
            "nan": lambda w: w["baseline"][2].__setitem__(50000, float("nan")),
            "infinity": lambda w: w["baseline"][2].__setitem__(50000, float("inf")),
            "clipping": lambda w: w["baseline"][2].__setitem__(50000, 1.1),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                waves = copy.deepcopy(self.waves)
                mutate(waves)
                with self.assertRaises(ValueError):
                    validator.verify_audio(self.contract, waves)

    def test_riff_reader_rejects_truncation_and_unsupported_data(self):
        fmt = struct.pack("<HHIIHH", 3, 2, 48000, 48000 * 8, 8, 32)
        audio = struct.pack("<ffff", 0.0, 0.25, -0.25, 0.0)
        body = b"WAVEfmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(audio)) + audio
        wav = b"RIFF" + struct.pack("<I", len(body)) + body
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "test.wav"
            path.write_bytes(wav)
            self.assertEqual(validator.read_wave(path), (48000, 2, [0.0, 0.25, -0.25, 0.0]))
            for bad in (wav[:-1], b"not a wave", wav.replace(b"data", b"none"),
                        wav[:20] + struct.pack("<H", 17) + wav[22:]):
                path.write_bytes(bad)
                with self.assertRaises(ValueError):
                    validator.read_wave(path)


if __name__ == "__main__":
    unittest.main()
