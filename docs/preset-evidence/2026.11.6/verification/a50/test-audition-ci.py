#!/usr/bin/env python3
"""Synthetic identity and upload negative controls; no actual artifact is qualified."""
import copy
import importlib.util
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('binder', HERE / 'bind-audition-ci.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


def fixture():
    report = dict(accepted_archive_content=True, status='accepted_actual_archive_content',
        verifier_sha256=b.content.file_sha(HERE / 'verify-audition-archive.py'),
        source_commit=b.SOURCE, source_tree=b.TREE, run_id=str(b.RUN), run_attempt='1',
        expected_ci_identity_checked=True, wav_count=225, selected_program_count=89,
        actual_audio_seconds=1276.2, actual_raw_audio_bytes=490060800, regular_files=276,
        checksum_count=275, nonfinite_samples=0, samples_over_full_scale=0,
        inventory_match={'values':106692}, zip_sha256='a'*64, zip_bytes=1234,
        capture_binary_sha256='b'*64)
    run = dict(id=b.RUN, run_attempt=1, head_sha=b.SOURCE, repository={'full_name':b.REPO})
    names = ['Run actions/checkout@v5','Build','Test','Capture curated preset auditions','Upload curated preset auditions']
    steps = [dict(name=name, number=i, status='completed', conclusion='success',
                  started_at=f'2026-11-24T10:0{i}:00Z', completed_at=f'2026-11-24T10:0{i}:50Z') for i,name in enumerate(names,1)]
    job = dict(id=b.JOB, run_id=b.RUN, run_attempt=1, head_sha=b.SOURCE, name='Linux / VST3 + Standalone',
               status='completed', conclusion='success', completed_at='2026-11-24T10:06:00Z', steps=steps)
    name = 'cs80-curated-preset-audition-' + b.SOURCE
    artifact = dict(id=12345, name=name, expired=False, workflow_run={'id':b.RUN,'head_sha':b.SOURCE},
        digest='sha256:'+'a'*64, size_in_bytes=1234, url=f'https://api.github.com/repos/{b.REPO}/actions/artifacts/12345',
        created_at='2026-11-24T10:05:41Z', expires_at='2027-02-01T00:00:00Z')
    log = '\n'.join([
        '2026-11-24T10:04:00.001Z python3 scripts/capture-factory-auditions.py --capture build/cs80_preset_capture --out preset-audition',
        '2026-11-24T10:05:40.010Z SHA256 digest of uploaded artifact zip is '+'a'*64,
        f'2026-11-24T10:05:40.020Z Artifact {name} has been successfully uploaded! Final size is 1234 bytes. Artifact ID is 12345',
        f'2026-11-24T10:05:40.030Z Artifact download URL: https://github.com/{b.REPO}/actions/runs/{b.RUN}/artifacts/12345'])
    return [report, artifact, run, job, log]


class BindingTests(unittest.TestCase):
    def test_synthetic_control_has_bounded_claims(self):
        result = b.bind(*fixture())
        self.assertFalse(result['release_qualified'])
        self.assertFalse(result['capture_binary_bytes_independently_available'])
        self.assertEqual(result['wav_count'],225)

    def test_content_mismatches(self):
        for key,value in [('accepted_archive_content',False),('verifier_sha256','0'*64),('source_commit','0'*40),
                          ('run_attempt','2'),('wav_count',171),('actual_raw_audio_bytes',396748800),
                          ('samples_over_full_scale',1),('inventory_match',{'values':105646})]:
            with self.subTest(key=key):
                args=fixture();args[0][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_artifact_mismatches(self):
        for key,value in [('expired',True),('name','old-audition'),('digest','sha256:'+'0'*64),
                          ('size_in_bytes',1233),('created_at','2026-11-24T09:00:00Z'),
                          ('workflow_run',{'id':b.RUN,'head_sha':'0'*40})]:
            with self.subTest(key=key):
                args=fixture();args[1][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_run_and_producer_mismatches(self):
        for index,key,value in [(2,'run_attempt',2),(2,'head_sha','0'*40),(3,'id',1),
                                (3,'conclusion','failure'),(3,'status','in_progress')]:
            with self.subTest(index=index,key=key):
                args=fixture();args[index][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_capture_and_upload_steps_must_succeed_once_in_order(self):
        for fault in ['failed_capture','skipped_upload','missing_capture','duplicate_capture','reordered']:
            with self.subTest(fault=fault):
                args=fixture();steps=args[3]['steps']
                if fault=='failed_capture':steps[-2]['conclusion']='failure'
                elif fault=='skipped_upload':steps[-1]['conclusion']='skipped'
                elif fault=='missing_capture':steps.pop(-2)
                elif fault=='duplicate_capture':steps.append(copy.deepcopy(steps[-2]))
                else:steps[-1]['number']=0
                with self.assertRaises(ValueError):b.bind(*args)

    def test_original_upload_must_bind_hash_id_size_time_and_command(self):
        for fault in ['digest','id','size','old_time','duplicate_digest','missing_command']:
            with self.subTest(fault=fault):
                args=fixture();log=args[-1]
                if fault=='digest':log=log.replace('a'*64,'c'*64)
                elif fault=='id':log=log.replace('ID is 12345','ID is 99999')
                elif fault=='size':log=log.replace('size is 1234','size is 1233')
                elif fault=='old_time':log=log.replace('10:05:40.','09:05:40.')
                elif fault=='duplicate_digest':log+='\n'+log.splitlines()[1]
                else:log='\n'.join(log.splitlines()[1:])
                args[-1]=log
                with self.assertRaises(ValueError):b.bind(*args)


if __name__=='__main__': unittest.main(verbosity=2)
