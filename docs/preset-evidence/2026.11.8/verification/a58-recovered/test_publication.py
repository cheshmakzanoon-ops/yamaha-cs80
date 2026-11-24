"""Synthetic contract tests plus a real historical receipt; no native acceptance."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

HERE = Path(__file__).parent
spec = importlib.util.spec_from_file_location('publication', HERE / 'prepare_publication.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class BindingChecks(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='cs80-a58-recovery-test-')
        self.addCleanup(self.tmp.cleanup)
        self.p = Path(self.tmp.name)
        self.binding = dict(source_commit='a'*40, source_tree='c'*40, run_id=987654321,
                            run_attempt=1, public_checker_sha256='b'*64,
                            reviewed_sound_source=m.SOUND_SOURCE, sound_source_unchanged=True)
        self.q = {k: self.binding[k] for k in ('source_commit','source_tree','run_id','run_attempt')}
        self.q.update(release_qualified=True, advertised_artifact_ids={}, native={'by_target':{}, 'totals':{}},
                      pluginval={'summary':dict(required_lanes=15,passed_lanes=15,
                                               validation_invocations=15,runtime_validation_retries=0)},
                      audition={'observed':copy.deepcopy(m.AUDITION)})
        self.run = dict(id=self.binding['run_id'],head_sha=self.binding['source_commit'],run_attempt=1,
                        status='completed',conclusion='success',workflow_id=399616500,
                        repository={'id':m.REPO_ID},head_repository={'id':m.REPO_ID})
        self.artifacts = []
        self.receipts = {}
        sums = {k:0 for k in m.COUNT_KEYS}
        sums['nested_python_skipped'] = None
        for i,(target,(name,label,formats,count)) in enumerate(m.TARGETS.items(),1):
            name = name or 'cs80-curated-preset-audition-'+self.binding['source_commit']
            self.q['advertised_artifact_ids'][target] = i
            if count:
                c = dict(registered=count,passed=count,failed=0,formal_skipped=0,disabled=0,unknown=0)
                self.q['native']['by_target'][target] = {'observed_counts':c}
                for key in m.COUNT_KEYS:sums[key] += c[key]
            route = f'https://api.github.com/repos/{m.REPO}/actions/artifacts/{i}'
            api = dict(id=i,name=name,url=route,archive_download_url=route+'/zip',expired=False,
                       expires_at='2100-02-22T19:00:00Z',size_in_bytes=10,digest='sha256:'+'d'*64,
                       workflow_run=dict(id=self.binding['run_id'],head_sha=self.binding['source_commit'],
                                         repository_id=m.REPO_ID,head_repository_id=m.REPO_ID))
            self.artifacts.append(api)
            self.receipts[target] = dict(source=self.binding['source_commit'],source_tree=self.binding['source_tree'],
                run_id=self.binding['run_id'],run_attempt=1,status='input_integrity_only',release_qualified=False,
                api_authenticity_verified=False,original_upload_attempt_verified=False,artifact=copy.deepcopy(api),
                checker={'bytes':1,'sha256':self.binding['public_checker_sha256']},
                public_get=dict(url=f'https://nightly.link/{m.REPO}/actions/artifacts/{i}.zip',status=200,
                                full_body_to_eof=True,bytes=10,sha256='d'*64,authorization=False,range=False))
        self.q['native']['totals'] = sums

    def execute(self):
        values={'binding':self.binding,'qualification':self.q,'run_api':self.run,
                'collection':dict(total_count=len(self.artifacts),artifacts=self.artifacts)}
        args = {}
        for key,value in values.items():
            path=self.p/(key+'.json');path.write_text(json.dumps(value));args[key]=path
        for target,value in self.receipts.items():
            (self.p/(target+'-public-receipt.json')).write_text(json.dumps(value))
        return m.validate(SimpleNamespace(**args,public_directory=self.p))

    def test_new_context_with_limited_raw_receipts(self):
        result=self.execute()
        self.assertFalse(result['release_qualified'])
        self.assertEqual(result['native_totals']['registered'],1446)
        self.assertIn('15:00:00 AST',result['expiry_display'])
        self.assertTrue(all(r['release_qualified'] is False for r in self.receipts.values()))

    def test_superseded_binding_refused(self):
        self.binding['source_commit']=m.SOUND_SOURCE
        with self.assertRaisesRegex(ValueError,'Explicit NEW source'):self.execute()

    def test_pending_aggregate_refused(self):
        self.q['release_qualified']=False
        with self.assertRaisesRegex(ValueError,'Separate completed'):self.execute()

    def test_failed_build_refused(self):
        self.run['conclusion']='failure'
        with self.assertRaisesRegex(ValueError,'Actual complete native'):self.execute()

    def test_unchanged_sound_assertion_required(self):
        self.binding['sound_source_unchanged']=False
        with self.assertRaisesRegex(ValueError,'unchanged sound-source'):self.execute()

    def test_old_public_context_cannot_be_mixed(self):
        self.receipts['macos-arm64']['source']=m.SOUND_SOURCE
        with self.assertRaisesRegex(ValueError,'Recovered receipt context'):self.execute()

    def test_range_only_and_wrong_digest_refused(self):
        self.receipts['audition']['public_get']['range']=True
        with self.assertRaisesRegex(ValueError,'non-range GET'):self.execute()
        self.receipts['audition']['public_get']['range']=False
        self.receipts['audition']['public_get']['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'complete bytes differ'):self.execute()

    def test_native_failures_and_old_audition_refused(self):
        self.q['native']['by_target']['windows-x86_64']['observed_counts']['failed']=1
        with self.assertRaisesRegex(ValueError,'Native accounting'):self.execute()
        self.q['native']['by_target']['windows-x86_64']['observed_counts']['failed']=0
        self.q['audition']['observed']['wav_count']=238
        with self.assertRaisesRegex(ValueError,'Actual audition dimensions'):self.execute()

    def test_real_historical_receipt_rejected_for_successor(self):
        path=HERE/'historical-public-receipt.json'
        self.receipts['macos-arm64']=json.loads(path.read_bytes())
        with self.assertRaisesRegex(ValueError,'Recovered receipt context'):self.execute()


if __name__=='__main__':unittest.main()
