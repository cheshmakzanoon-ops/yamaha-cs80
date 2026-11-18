"""No DSP/build/network: positive receipt replay plus deliberately corrupted copies."""
from pathlib import Path
import copy, tempfile, shutil, unittest
import verify
ROOT=Path(__file__).resolve().parent
class ReplayTests(unittest.TestCase):
    def setUp(self):self.data=verify.load(ROOT)
    def bad(self,fn):
        d=copy.deepcopy(self.data);fn(d)
        with self.assertRaises(verify.Invalid):verify.validate(d)
    def test_good_receipts(self):
        import reproduce
        self.assertEqual(verify.validate(self.data)['status'],'PASS receipt replay only')
        for name in reproduce.VECTOR:
            self.assertEqual('-ffp-contract=off' in reproduce.tu_flags(name),name in ('Ig00156Batch','WscNodeKernelAvx2'))
    def test_good_package(self):verify.verify_package(ROOT)
    def test_wrong_baseline(self):self.bad(lambda d:d['provenance.json'].__setitem__('baseline_commit','0'*40))
    def test_wrong_revision(self):self.bad(lambda d:d['provenance.json']['source_freezes'].__setitem__('V','0'*64))
    def test_swapped_archives(self):self.bad(lambda d:d['provenance.json']['normal_archives'].__setitem__('V',d['provenance.json']['normal_archives']['S']))
    def test_missing_scene(self):self.bad(lambda d:d['scalar-evidence.json']['scenes'].pop())
    def test_missing_comparison(self):self.bad(lambda d:d['scalar-evidence.json']['scenes'][0]['streams'][0]['matched_arms'].pop())
    def test_rng_total(self):self.bad(lambda d:d['scalar-evidence.json']['scenes'][0].__setitem__('rank_rng_draws',1))
    def test_wrong_group_ordinal(self):self.bad(lambda d:d['scalar-evidence.json']['scenes'][0]['groups'][0]['numbered'].__setitem__(2,0))
    def test_wrong_terminal_mask(self):self.bad(lambda d:d['scalar-evidence.json']['scenes'][0]['terminal'][0]['masks'].__setitem__(14,1))
    def test_scalar_as_vector_scope(self):self.bad(lambda d:d['scalar-evidence.json'].__setitem__('scope','integrated vector proof'))
    def test_fake_component_success(self):self.bad(lambda d:d['component-evidence.json']['vector_finish']['vector_matrix'].__setitem__('kernel_successes',9414))
    def test_missing_scalar_impostor_negative(self):self.bad(lambda d:d['component-evidence.json']['kernel_negatives'][-1].__setitem__('structural_packed_work_check',True))
    def test_no_consumption_negative(self):self.bad(lambda d:d['component-evidence.json']['vector_finish']['record_consumption_negative'].__setitem__('actual_exit',0))
    def test_critical_code_changed(self):self.bad(lambda d:next(iter(d['integrated-public-probes.json']['static']['critical_functions'].values())).__setitem__('bytes_equal',False))
    def test_missing_actual_trap(self):self.bad(lambda d:d['integrated-public-probes.json']['arms'][0].__setitem__('genuine_in_renderer_sigfpe',6))
    def test_fake_integrated_completion(self):self.bad(lambda d:d['integrator-hook.json']['integrated_V1b_full_trace'].__setitem__('status','PASS'))
    def test_fake_timing(self):self.bad(lambda d:d['integrator-hook.json']['cpu_timing'].__setitem__('receipt',{'speedup':12.3}))
    def test_premature_promotion(self):self.bad(lambda d:d['integrator-hook.json']['cpu_timing'].__setitem__('decision','PROMOTE'))
    def test_unconsumed_integrated_record(self):self.bad(lambda d:d['integrated-vector-evidence.json']['scenes'][0]['windows'][0]['by_group']['known'].__setitem__(0,0))
    def test_missing_integrated_supplement(self):self.bad(lambda d:d['integrated-vector-evidence.json']['supplements'].pop('blocks'))
    def test_file_tamper(self):
        with tempfile.TemporaryDirectory() as t:
            q=Path(t)/'package';shutil.copytree(ROOT,q,ignore=shutil.ignore_patterns('__pycache__'))
            with (q/'stock-to-scalar.patch').open('ab')as f:f.write(b'\n')
            with self.assertRaises(verify.Invalid):verify.verify_package(q)
if __name__=='__main__':unittest.main()
