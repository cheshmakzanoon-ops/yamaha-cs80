#!/usr/bin/env python3
"""Additive CPU producer-witness controls, preregistered before gate repair.

Counterfeits are test inputs only, never measured performance evidence. These
checks preserve the original benchmark source, workload and timing thresholds.
"""
from __future__ import annotations

import copy
import csv
import hashlib
import json
from pathlib import Path
import unittest

from test_numerical_release_gate import CONTRACT, ROOT, TEST_CONTRACT, fixture, gate


class CpuWorkloadWitnessTests(unittest.TestCase):
    def test_frozen_real_round_is_accepted_as_integrity_not_performance(self):
        directory = ROOT / 'docs/evidence/qualification-2026-10-31/cpu/stationary-controls/round-1'
        hashes = {
            'metadata.json': '25954954dbedaa6413845902c3d5fb1dbbb6e85f7cd77464e31d8105c16d8e6d',
            'completed.json': '1e7d20b1afb2139fe82f8f8dfa0dfd6facac4e104e7d800398fd3e0db241985c',
            'results.csv': '2796f381f5ae4228978bd213942be0fe79e62034c1cd7669d245be070b6fa06e',
        }
        for name, expected in hashes.items():
            self.assertEqual(hashlib.sha256((directory / name).read_bytes()).hexdigest(), expected)
        rd = copy.deepcopy(fixture()['cpu']['before'][0])
        rd['metadata'] = json.loads((directory / 'metadata.json').read_text())
        rd['completed'] = json.loads((directory / 'completed.json').read_text())
        with (directory / 'results.csv').open(newline='') as stream:
            rd['rows'] = list(csv.DictReader(stream))
        source = rd['metadata']['source_commit_caller_verified']
        # Receipt identity is synthetic here. Raw historical timings remain
        # untouched and above the performance ceilings; this is not a release.
        rd['receipt']['source_sha'] = source
        self.assertEqual(len(gate.verify_cpu_round(CONTRACT, rd, source, 'sealed-real-control')), 12)

    def test_consistently_counterfeit_metadata_must_fail(self):
        for field, bad in (
            ('warmup_seconds', 0), ('tail_minimum_seconds', 0),
            ('tail_cap_seconds', 0), ('performance_fx_tail_seconds', 0),
            ('performance_pedal_hold_seconds', 0), ('performance_include_64', True),
            ('timed_scope', 'render only, omitted frame and control events'),
        ):
            with self.subTest(field=field):
                candidate = fixture()
                for rd in candidate['cpu']['before'] + candidate['cpu']['after']:
                    rd['metadata'][field] = bad
                with self.assertRaisesRegex(gate.GateError, 'registered workload metadata'):
                    gate.validate_evidence(TEST_CONTRACT, candidate)

    def test_all_original_workload_witnesses_are_enforced(self):
        def select(bundle, *, scenario='paired_tones', mode='isolation', phase='warmed_held'):
            return next(row for row in bundle['cpu']['after'][0]['rows']
                        if row['scenario'] == scenario and row['mode'] == mode and row['phase'] == phase)

        cases = [
            ('held voice minimum', {}, {'min_active_voices': '0'}),
            ('held voice maximum', {}, {'max_active_voices': '7'}),
            ('held voice final', {}, {'final_active_voices': '0'}),
            ('held shortened callbacks', {}, {'callbacks': '1023'}),
            ('held increased callbacks', {}, {'callbacks': '1025'}),
            ('legacy callback omission', {'mode': 'legacy'}, {'callbacks': '0'}),
            ('idle contains voices', {'scenario': 'idle_quiet'}, {'min_active_voices': '8'}),
            ('moving callback omission', {'scenario': 'moving_tabs_pressure_wah'}, {'callbacks': '0'}),
            ('moving selector omission', {'scenario': 'moving_tabs_pressure_wah'}, {'selector_events': '0'}),
            ('moving source compilation omission', {'scenario': 'moving_tabs_pressure_wah'}, {'source_compilations': '0'}),
            ('static source recompilation', {}, {'source_compilations': '1'}),
            ('unexpected MPE work', {}, {'mpe_member_state_checks': '8'}),
            ('unexpected controller work', {}, {'controller_events': '1'}),
            ('unexpected pedal work', {}, {'pedal_held_callbacks': '1'}),
            ('silent held fixture', {}, {'peak': '0'}),
            ('shortened release observation', {'phase': 'release_tail'}, {'callbacks': '1'}),
            ('extended release observation', {'phase': 'release_tail'}, {'callbacks': '7501'}),
            ('premature incomplete tail', {'phase': 'release_tail'}, {'tail_voices_complete': '0'}),
            ('tail completion with active voices', {'phase': 'release_tail'}, {'final_active_voices': '8'}),
        ]
        for label, selector, mutation in cases:
            with self.subTest(label=label):
                candidate = fixture()
                select(candidate, **selector).update(mutation)
                with self.assertRaisesRegex(gate.GateError, 'workload witness'):
                    gate.validate_evidence(TEST_CONTRACT, candidate)

    def test_original_cpu_csv_schema_cannot_drop_observations(self):
        for missing in ('min_active_voices', 'thread_cpu_seconds_including_checks', 'p95_us'):
            with self.subTest(missing=missing):
                candidate = fixture()
                for rd in candidate['cpu']['before'] + candidate['cpu']['after']:
                    for row in rd['rows']:
                        row.pop(missing)
                with self.assertRaisesRegex(gate.GateError, 'original ordered CSV header'):
                    gate.validate_evidence(TEST_CONTRACT, candidate)


if __name__ == '__main__':
    unittest.main()
