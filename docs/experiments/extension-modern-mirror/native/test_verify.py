#!/usr/bin/env python3
"""Synthetic mutations of stored records only. Never loads a plugin or renderer."""
import copy,json,struct,unittest
import verify as V
class Tests(unittest.TestCase):
    def setUp(self):self.b,self.e,self.rows=V.load()
    def test_completed_package(self):self.assertTrue(V.verify()['passed'])
    def test_nan_PCM(self):
        x=bytearray(self.b['pcm.bin']);struct.pack_into('<I',x,0,0x7fc00001);self.b['pcm.bin']=bytes(x)
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def test_wrong_mode(self):
        x=bytearray(self.b['callbacks.bin']);struct.pack_into('<I',x,16*72+4,0);self.b['callbacks.bin']=bytes(x)
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def test_wrong_FP_control(self):
        x=bytearray(self.b['callbacks.bin']);struct.pack_into('<I',x,40,0xbfc0);self.b['callbacks.bin']=bytes(x)
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def test_missing_callback(self):
        self.b['callbacks.bin']=self.b['callbacks.bin'][:-72]
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def mutate_counts(self,index,new):
        rows=list(map(json.loads,self.b['observer-counts.jsonl'].splitlines()));rows[index]['after']=new
        self.b['observer-counts.jsonl']=('\n'.join(map(json.dumps,rows))+'\n').encode()
    def test_false_hit_8mode(self):
        self.mutate_counts(0,[1,1,1])
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def test_missing_first16_hit(self):
        self.mutate_counts(16,[0,0,0])
        with self.assertRaises(AssertionError):V.validate_callbacks(self.b)
    def test_invalid_shape(self):
        key='8-config-1-after.state';raw=self.b[key];at=raw.index(b'<MODERN_LAYER');tail=raw[at:];assert b'0.25' in tail;self.b[key]=raw[:at]+tail.replace(b'0.25',b'0.26',1)
        with self.assertRaises(AssertionError):V.validate_states(self.b)
    def test_private_trailer_change(self):
        key='8-config-1-input.state';self.b[key]+=b'x'
        with self.assertRaises(AssertionError):V.validate_states(self.b)
    def test_wrong_recorded_digest(self):
        self.e['arms'][2]['files']['pcm.bin']['sha256']='0'*64
        with self.assertRaises(AssertionError):V.validate_identity(self.b,self.e)
    def test_modified_csv(self):
        rows=copy.deepcopy(self.rows);rows[48]['count_after_accept']='99';self.assertNotEqual(rows,V.validate_callbacks(self.b))
    def test_truncated_arm(self):
        self.e['arms'][3]['files'].pop('pcm.bin')
        with self.assertRaises(AssertionError):V.validate_identity(self.b,self.e)
if __name__=='__main__':unittest.main(verbosity=2)
