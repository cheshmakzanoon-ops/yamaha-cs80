#!/usr/bin/env python3
"""Synthetic record mutations only; no native execution."""
import json,struct,unittest
import verify as V
class Tests(unittest.TestCase):
    def setUp(self):self.b,self.e=V.load()
    def test_complete_package(self):self.assertTrue(V.verify()['passed'])
    def test_PCM_corruption(self):
        a=bytearray(self.b['pcm.bin']);struct.pack_into('<I',a,0,0x7fc00001);self.b['pcm.bin']=bytes(a)
        with self.assertRaises(AssertionError):V.public(self.b,self.e)
    def test_missing_callback(self):
        self.b['callbacks.bin']=self.b['callbacks.bin'][:-72]
        with self.assertRaises(AssertionError):V.public(self.b,self.e)
    def test_FP_control_change(self):
        a=bytearray(self.b['callbacks.bin']);struct.pack_into('<I',a,40,0xbfc0);self.b['callbacks.bin']=bytes(a)
        with self.assertRaises(AssertionError):V.public(self.b,self.e)
    def test_extra_event(self):
        r=list(map(json.loads,self.b['inputs.jsonl'].splitlines()));r[9]['event_reads']=1;self.b['inputs.jsonl']=('\n'.join(map(json.dumps,r))+'\n').encode()
        with self.assertRaises(AssertionError):V.public(self.b,self.e)
    def test_count_drift(self):
        r=list(map(json.loads,self.b['observer-counts.jsonl'].splitlines()));r[1024]['before'][1]+=1;self.b['observer-counts.jsonl']=('\n'.join(map(json.dumps,r))+'\n').encode()
        with self.assertRaises(AssertionError):V.counts(self.b,self.e)
    def test_forged_aggregate(self):
        self.e['attribution']['phases']['window']['accepted_eight_calls']+=1
        with self.assertRaises(AssertionError):V.counts(self.b,self.e)
    def test_arm_digest_mismatch(self):
        self.e['arms'][2]['files']['pcm.bin']['sha256']='0'*64
        with self.assertRaises(AssertionError):V.identity(self.b,self.e)
if __name__=='__main__':unittest.main(verbosity=2)
