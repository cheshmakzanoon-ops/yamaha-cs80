"""Synthetic verifier controls only; these are never real-host evidence."""
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
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


class ProvenanceControls(unittest.TestCase):
    """Intentionally artificial metadata fixtures; not a host acceptance result."""
    contract_file = "contract-v1.json"

    def test_identity_state_and_failure_mutations(self):
        contract = json.loads((ROOT / "tests/hosts/reaper" / self.contract_file).read_text())
        state = {"VOLUME": 0.55, "VOICE DISPERSION": 0, "ANALOG DRIFT": 0, "ANALOG NOISE": 0}
        if self.contract_file == "contract-v2-dry.json":
            state["CHORUS ON"] = 0
        notes = [{"pitch": n, "velocity": 100, "start_seconds": 0.25, "end_seconds": 1.5}
                 for n in contract["render"]["notes"]]
        observed = {"schema": contract["schema"], "host_version": "7.84/linux-x86_64",
                    "checks": [{"ok": True}], "scan_matches": [{"name": "synthetic unit fixture"}],
                    "fx_name": "CS-80 Reference", "fx_ident": "unit-fixture-only", "editor_open": True,
                    "pdc_samples": 67, "chain_reported_pdc_samples": 67,
                    "before_save": state, "after_reopen": state,
                    "perturbed": {"VOLUME": 0.2, "VOICE DISPERSION": 0.7},
                    "projects": {name: {"notes": notes} for name in ("baseline", "restored", "automated")}}
        observed["projects"]["automated"]["envelope"] = contract["render"]["automation_points"]
        observed["projects"]["no-midi"] = {"notes": []}
        if self.contract_file == "contract-v2-dry.json":
            observed["editor_open_after_escape"] = True
        source, tree = "1" * 40, "2" * 40
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "contract-v1.json").write_text(json.dumps(contract))
            manifest = {"source_commit": source, "src_tree": tree,
                        "host_archive_sha256": contract["host"]["sha256"],
                        "plugin_build_sha256": "3" * 64, "plugin_installed_sha256": "3" * 64,
                        "contract_sha256": validator.sha256(out / "contract-v1.json"),
                        "exit_codes": {name: 0 for name in ["setup"] + contract["render"]["files"]},
                        "files": {}}
            # Establish the metadata-only positive before trying negatives. No
            # fake WAV, PNG or successful host receipt is generated here.
            contract_hash = validator.sha256(out / "contract-v1.json")
            self.assertIsNone(validator.verify_metadata(manifest, observed, contract, source, tree, contract_hash))
            mutations = {
                "source": ("checkout source mismatch", lambda m, o: m.update(source_commit="4" * 40)),
                "src-tree": ("source tree mismatch", lambda m, o: m.update(src_tree="4" * 40)),
                "host-hash": ("host archive identity mismatch", lambda m, o: m.update(host_archive_sha256="4" * 64)),
                "plugin-hash": ("built/installed plugin mismatch", lambda m, o: m.update(plugin_installed_sha256="4" * 64)),
                "contract-hash": ("contract hash mismatch", lambda m, o: m.update(contract_sha256="4" * 64)),
                "runner-failure": ("host runner reported failure", lambda m, o: m.update(error="timed out")),
                "wrong-host-version": ("running host version mismatch", lambda m, o: o.update(host_version="0.0/linux-x86_64")),
                "failed-assertion": ("host assertions failed/missing", lambda m, o: o.update(checks=[{"ok": False}])),
                "missing-assertions": ("host assertions failed/missing", lambda m, o: o.update(checks=[])),
                "editor-closed": ("host editor did not open", lambda m, o: o.update(editor_open=False)),
                "nonfinite-pdc": ("invalid host-reported latency", lambda m, o: o.update(pdc_samples=float("nan"))),
                "wrong-restored-state": ("wrong restored VOLUME", lambda m, o: o["after_reopen"].update(VOLUME=0.2)),
                "missing-midi": ("lost MIDI notes", lambda m, o: o["projects"]["restored"].update(notes=[])),
                "lost-envelope": ("changed persisted envelope", lambda m, o: o["projects"]["automated"].update(envelope=[])),
            }
            if self.contract_file == "contract-v2-dry.json":
                mutations["restored-chorus-enabled"] = ("wrong restored CHORUS ON", lambda m, o: o["after_reopen"].update({"CHORUS ON": 1}))
                mutations["editor-closed-after-escape"] = ("host editor closed during tour dismissal", lambda m, o: o.update(editor_open_after_escape=False))
            for name, (reason, mutate) in mutations.items():
                with self.subTest(name=name):
                    m, o = copy.deepcopy(manifest), copy.deepcopy(observed)
                    mutate(m, o)
                    with self.assertRaisesRegex(ValueError, "^" + re.escape(reason) + "$"):
                        validator.verify_metadata(m, o, contract, source, tree, contract_hash)


class DryProvenanceControls(ProvenanceControls):
    contract_file = "contract-v2-dry.json"


class DryContractControls(unittest.TestCase):
    def test_copied_contract_filename_is_versioned(self):
        self.assertEqual(validator.evidence_contract_filename({"schema": "cs80-reaper-smoke-v1"}), "contract-v1.json")
        self.assertEqual(validator.evidence_contract_filename({"schema": "cs80-reaper-smoke-v2-dry"}), "contract-v2-dry.json")
        with self.assertRaisesRegex(ValueError, "^unsupported host contract schema$"):
            validator.evidence_contract_filename({"schema": "unregistered"})

    def test_dry_fixture_preserves_original_contract_and_all_acceptance_limits(self):
        original = (ROOT / "tests/hosts/reaper/contract-v1.json").read_bytes()
        self.assertEqual(hashlib.sha256(original).hexdigest(),
                         "e0fb81082eaac5989bc25655f0f9cef4a9c9930b9c2a1193624bdc469b1ec362")
        v1 = json.loads(original)
        v2 = json.loads((ROOT / "tests/hosts/reaper/contract-v2-dry.json").read_text())
        self.assertEqual(v2["limits"], v1["limits"])
        self.assertEqual(v2["host"], v1["host"])
        self.assertEqual(v2["excluded_claims"], v1["excluded_claims"])
        render = v2["render"].copy()
        self.assertEqual(render.pop("chorus_on_normalized"), 0)
        self.assertEqual(render, v1["render"])


if __name__ == "__main__":
    unittest.main()
