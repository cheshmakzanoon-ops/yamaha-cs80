"""Offline corruption tests. No compiler, synth, profiler or raw capture access."""
import copy
import json
import unittest
import verify as v

class ReceiptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files=v.read_tree(v.ROOT)
        cls.docs=v.check_bundle(cls.files)
    def bad(self, edit):
        data=copy.deepcopy(self.docs); edit(data)
        with self.assertRaises((ValueError,KeyError,TypeError)):v.check_records(data)
    def test_good_receipts(self):v.check_records(self.docs)
    def test_changed_patch(self):
        files=dict(self.files); files['candidate.patch']+=b'X'
        with self.assertRaises(ValueError):v.check_bundle(files)
    def test_changed_receipt_bytes(self):
        files=dict(self.files); files['records/direct.json']+=b' '
        with self.assertRaises(ValueError):v.check_bundle(files)
    def test_missing_file(self):
        files=dict(self.files); del files['records/engine.json']
        with self.assertRaises(ValueError):v.check_bundle(files)
    def test_extra_file(self):
        files=dict(self.files); files['unbound.txt']=b'x'
        with self.assertRaises(ValueError):v.check_bundle(files)
    def test_duplicate_json_key(self):
        with self.assertRaises(ValueError):v.decode(b'{"x":1,"x":2}')
    def test_wrong_arm(self):self.bad(lambda d:d['provenance.json'].update(arm='S'))
    def test_wrong_component(self):self.bad(lambda d:d['provenance.json'].update(component_cpp_sha256='0'*64))
    def test_wrong_archive(self):self.bad(lambda d:d['records/fixture.json'].update(normal_archive_sha256='0'*64))
    def test_false_critical_gate(self):self.bad(lambda d:d['records/engine.json']['critical_isolation']['critical'][1].update(bytes_identical=False))
    def test_missing_scene(self):self.bad(lambda d:d['records/engine.json']['scenes'].pop('partial_three'))
    def test_rng_not_just_final_state(self):self.bad(lambda d:d['records/engine.json']['scenes']['factory_held'].update(actual_rank_draws=1))
    def test_rank_chronology(self):self.bad(lambda d:d['records/engine.json']['scenes']['factory_held']['late'].update(returns=262143))
    def test_fake_all_ordinary_seed(self):self.bad(lambda d:d['records/engine.json']['seed_controls']['cases'][1].update(terminal_hits=32))
    def test_missing_actual_signal(self):self.bad(lambda d:d['records/engine.json']['public_traps'].update(real_sigfpe_cases=0))
    def test_arbitrary_link_claim(self):self.bad(lambda d:d['records/independent.json']['weak_disposition'].update(arbitrary_link_safety_claimed=True))
    def test_directed_mismatch(self):self.bad(lambda d:d['records/independent.json']['directed']['arms']['reverse'].update(sha256='0'*64))
    def test_false_pure_hit(self):self.bad(lambda d:d['records/fixture.json']['scenes']['pure-held']['actual_counts'].update(window_entries=1))
    def test_warmup_in_window(self):self.bad(lambda d:d['records/fixture.json']['scenes']['factory-held']['actual_counts'].update(window_entries=3145728))
    def test_truncated_full_fixture(self):self.bad(lambda d:d['records/fixture.json'].update(window_blocks=128))
    def test_performance_claim(self):self.bad(lambda d:d['records/work-count.json'].update(C_performance_admitted=True))
    def test_borrowed_cost(self):self.bad(lambda d:d['records/work-count.json'].update(S_V_costs_attributed_to_C=True))
    def test_uncollected_work_count(self):self.bad(lambda d:d['records/work-count.json']['feasibility'].update(instruction_counts_collected=True))
    def test_synthetic_inventory_corruption(self):
        expected={'core/a':v.sha(b'a\n')}
        for files in ({'core/a':b'b\n'},{},{'core/a':b'a\n','extra':b''}):
            with self.assertRaises(ValueError):v.check_inventory(files,expected)

class PatchTests(unittest.TestCase):
    patch=b'--- a/src/core/a\n+++ b/src/core/a\n@@ -1,2 +1,2 @@\n a\n-b\n+c\n--- /dev/null\n+++ b/src/core/new\n@@ -0,0 +1 @@\n+n\n'
    source={'core/a':b'a\nb\n'}
    def test_exact_patch(self):self.assertEqual(v.apply_patch(self.source,self.patch),{'core/a':b'a\nc\n','core/new':b'n\n'})
    def test_context_not_fuzzy(self):
        with self.assertRaises(ValueError):v.apply_patch({'core/a':b'other\nb\n'},self.patch)
    def test_unsafe_path(self):
        with self.assertRaises(ValueError):v.apply_patch(self.source,self.patch.replace(b'b/src/core/a',b'b/src/../a'))
    def test_wrong_offset(self):
        with self.assertRaises(ValueError):v.apply_patch(self.source,self.patch.replace(b'@@ -1,2 +1,2',b'@@ -1,2 +2,2'))
    def test_truncated_patch(self):
        with self.assertRaises(ValueError):v.apply_patch(self.source,self.patch[:-3])
    def test_wrong_hunk_count(self):
        with self.assertRaises(ValueError):v.apply_patch(self.source,self.patch.replace(b'@@ -1,2 +1,2',b'@@ -1,3 +1,2'))

if __name__=='__main__':unittest.main()
