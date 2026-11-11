#!/usr/bin/env python3
"""Independent RIFF chunk walk, float bit oracle and CLI artifact contracts."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

p = argparse.ArgumentParser()
p.add_argument('--filter', required=True)
p.add_argument('--vca', required=True)
p.add_argument('--fixture', required=True)
p.add_argument('--baseline-filter')
p.add_argument('--baseline-vca')
a, rest = p.parse_known_args()
ROOT = Path(__file__).resolve().parents[1]


def read_wav(path):
    raw = path.read_bytes()
    magic, length, kind = struct.unpack_from('<4sI4s', raw)
    assert magic == b'RIFF' and kind == b'WAVE' and length == len(raw)-8
    pos, chunks = 12, {}
    while pos < len(raw):
        tag, size = struct.unpack_from('<4sI', raw, pos)
        assert tag not in chunks and pos+8+size <= len(raw)
        chunks[tag] = raw[pos+8:pos+8+size]
        pos += 8+size+(size & 1)
    assert pos == len(raw) and list(chunks) == [b'fmt ', b'fact', b'data']
    fmt = struct.unpack('<HHIIHHH', chunks[b'fmt '])
    assert fmt[0:2] == (3, 1) and fmt[3:] == (fmt[2]*4, 4, 32, 0)
    count, = struct.unpack('<I', chunks[b'fact'])
    assert len(chunks[b'data']) == 4*count
    return fmt[2], count, chunks[b'data'], struct.unpack('<'+'f'*count, chunks[b'data'])


class WavTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.drive = self.root/'drive.csv'
        self.drive.write_text('sample,input,lc,ei,envelope\n'+''.join(
            f'{i},{math.sin(i/8):.17g},1,0,{0 if i == 0 else 1}\n' for i in range(65)))
        self.input = self.root/'input.csv'
        self.input.write_text('sample,value\n'+''.join(f'{i},{math.sin(i/8):.17g}\n' for i in range(65)))
        self.controls = self.root/'controls.csv'
        self.controls.write_text('sample,hp_hz,lp_hz,q\n'+''.join(f'{i},200,{2000+i},1\n' for i in range(65)))

    def run_tool(self, tool, name, extra=(), compare=True, binary=None, success=True):
        out = self.root/name
        if tool == 'filter':
            args = ['--input',str(self.input),'--input-rate','48000','--input-samples','65',
                    '--controls',str(self.controls)]
            if compare: args += ['--compare-realtime','true','--realtime-rate','48000']
        else:
            args = ['--input',str(self.drive),'--rate','48000','--samples','65']
            if compare: args += ['--compare-realtime','true']
        result = subprocess.run([binary or getattr(a,tool),'--out',str(out),*args,*extra],capture_output=True,text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout+result.stderr)
        if not success: self.assertFalse(out.exists())
        return out

    def verify(self, out, tool, gain, compare):
        meta = json.loads((out/'manifest.json').read_text())
        audio = meta['audio_export']
        self.assertEqual(audio['shared_gain'],gain)
        self.assertEqual(meta['source_sha256']['wav_helper'],hashlib.sha256((ROOT/'src/tools/FloatWav.h').read_bytes()).hexdigest())
        self.assertEqual(audio['sample_count'],65)
        self.assertEqual(audio['endpoint_span_s'],64/48000)
        self.assertEqual(audio['playback_duration_s'],65/48000)
        for mode in ['reference']+(['realtime'] if compare else []):
            rate,count,data,values = read_wav(out/(mode+'.wav'))
            self.assertEqual((rate,count),(48000,65))
            column = ('lp' if mode == 'reference' else 'lp_realtime') if tool == 'filter' else ('output' if mode == 'reference' else 'realtime_output')
            source = out/('realtime.csv' if tool == 'filter' and mode == 'realtime' else 'trace.csv')
            with source.open() as f: expected = b''.join(struct.pack('<f',float(row[column])*gain) for row in csv.DictReader(f))
            self.assertEqual(data,expected)
            stats = audio[mode]
            self.assertEqual(stats['float32_peak_abs'],max(map(abs,values)))
            self.assertEqual(stats['above_full_scale_samples'],sum(abs(v)>1 for v in values))
            if gain == 100: self.assertGreater(stats['above_full_scale_samples'],0)
            if gain == 0: self.assertTrue(all(v == 0 for v in values))
        if not compare: self.assertFalse((out/'realtime.wav').exists())

    def test_samples_shared_gain_and_unchanged_csv(self):
        for tool in ['filter','vca']:
            off = self.run_tool(tool,tool+'-off')
            self.assertNotIn('audio_export',json.loads((off/'manifest.json').read_text()))
            self.assertFalse(list(off.glob('*.wav')))
            baseline = getattr(a,'baseline_'+tool)
            if baseline:
                old = self.run_tool(tool,tool+'-old',binary=baseline)
                for f in old.glob('*.csv'): self.assertEqual(f.read_bytes(),(off/f.name).read_bytes())
                old_meta=json.loads((old/'manifest.json').read_text())
                new_meta=json.loads((off/'manifest.json').read_text())
                del old_meta['source_sha256']; del new_meta['source_sha256']
                self.assertEqual(old_meta,new_meta)
            for gain in [0,.125,100,1e6]:
                on = self.run_tool(tool,tool+str(gain),['--wav-gain',str(gain)])
                self.verify(on,tool,gain,True)
                for f in off.glob('*.csv'): self.assertEqual(f.read_bytes(),(on/f.name).read_bytes())
                before=json.loads((off/'manifest.json').read_text())
                after=json.loads((on/'manifest.json').read_text()); del after['audio_export']
                self.assertEqual(before,after)
            again = self.run_tool(tool,tool+'-again',['--wav-gain','.125'])
            for f in again.iterdir(): self.assertEqual(f.read_bytes(),(self.root/(tool+'0.125')/f.name).read_bytes())
            single = self.run_tool(tool,tool+'-single',['--wav-gain','.125'],compare=False)
            self.verify(single,tool,.125,False)

    def test_silent_source(self):
        self.input.write_text('sample,value\n'+''.join(f'{i},0\n' for i in range(65)))
        self.drive.write_text('sample,input,lc,ei,envelope\n'+''.join(f'{i},0,0,0,0\n' for i in range(65)))
        for tool in ['filter','vca']:
            out=self.run_tool(tool,tool+'-silent',['--wav-gain','1'])
            self.verify(out,tool,1,True)
            for file in out.glob('*.wav'):
                self.assertTrue(all(x == 0 for x in read_wav(file)[3]))

    def test_invalid_arguments(self):
        for tool in ['filter','vca']:
            for i,value in enumerate(['-1','1000001','nan','inf','1e999','abc','']):
                self.run_tool(tool,tool+str(i),['--wav-gain',value],success=False)
            self.run_tool(tool,tool+'duplicate',['--wav-gain','1','--wav-gain','2'],success=False)
            self.run_tool(tool,tool+'missing',['--wav-gain'],success=False)

    def test_helper_bits_bounds_no_overwrite_and_failure_cleanup(self):
        path=self.root/'fixture.wav'
        def run(mode, **kwargs):
            return subprocess.run([a.fixture,str(path),mode],capture_output=True,text=True,**kwargs)
        self.assertEqual(run('normal').returncode,0)
        rate,count,data,values=read_wav(path)
        expected=b''.join(struct.pack('<f',v) for v in [0.,-0.,.25,-.25,2.,-2.,1.00000001])*100
        self.assertEqual(data,expected)
        self.assertEqual((rate,count),(48000,700))
        original=path.read_bytes()
        self.assertNotEqual(run('normal').returncode,0)
        self.assertEqual(path.read_bytes(),original)
        path.unlink()
        for mode in ['nonfinite','overflow','size','rate']:
            self.assertNotEqual(run(mode).returncode,0)
            self.assertFalse(path.exists())
        try:
            import resource
            import signal
        except ImportError:
            self.skipTest('POSIX file-size fault injection unavailable')
        def limit():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(100,100))
        result=run('normal',preexec_fn=limit)
        self.assertNotEqual(result.returncode,0,result.stderr)
        self.assertFalse(path.exists(),result.stderr)

if __name__ == '__main__':
    unittest.main(argv=[__file__,*rest])
