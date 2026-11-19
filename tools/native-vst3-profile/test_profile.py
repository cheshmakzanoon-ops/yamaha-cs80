#!/usr/bin/env python3
"""Offline bundle replay fault controls; does not load a plug-in, sample or compile."""
from copy import deepcopy
import unittest
# Avoid shadowing the standard library 'profile' module in outside test runners.
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
spec = spec_from_file_location('native_pcm_profile', Path(__file__).with_name('profile.py'))
tool = module_from_spec(spec); spec.loader.exec_module(tool)

class ReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = tool.load(tool.DEFAULT)

    def test_complete(self):
        rows = tool.summary(self.data)
        self.assertEqual([len(rows), sum(r['callback_pcs'] for r in rows), sum(r['calibration_pcs'] for r in rows)],
                         [12, 6278, 296])

    def bad(self, edit):
        d=deepcopy(self.data);edit(d)
        with self.assertRaises(ValueError): tool.summary(d)

    def test_omitted_slot(self): self.bad(lambda d: d['captures'].pop())
    def test_wrong_module(self): self.bad(lambda d: d['captures'][0]['sampler'].update(plugin_build_id='cafe'))
    def test_timer_overflow(self): self.bad(lambda d: d['captures'][0]['sampler'].update(dropped=1))
    def test_foreign_delivery(self): self.bad(lambda d: d['captures'][0]['sampler'].update(foreign_signals=1))
    def test_lost_pc(self): self.bad(lambda d: d['captures'][2]['pcs'].pop())
    def test_duplicate_pc(self): self.bad(lambda d: d['captures'][0]['pcs'].append(d['captures'][0]['pcs'][0]))
    def test_quiet_guard(self): self.bad(lambda d: d['captures'][1]['scene'].update(quiet_guard_peak=.2))
    def test_missing_note_off(self): self.bad(lambda d: d['captures'][1]['scene'].update(release_note_off_reads=0))
    def test_bad_control_pcm(self): self.bad(lambda d: d['captures'][-1]['scene'].update(region_pcm_fnv1a64='0'*16))
    def test_wrong_calibration_loop(self):
        def mutate(d):
            row=next(x for x in d['captures'][0]['pcs'] if x[0]==1)
            d['symbols'][row[1]][2]='other'
        self.bad(mutate)
    def test_bad_region(self):
        def mutate(d):
            row=next(x for x in d['captures'][0]['pcs'] if x[0]==2);row[0]=3
        self.bad(mutate)
    def test_negative_audio(self): self.bad(lambda d: d['captures'][2]['scene'].update(region_nonzero_samples=-1))

if __name__=='__main__': unittest.main()
