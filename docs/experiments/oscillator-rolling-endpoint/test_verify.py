"""Exercise public replay's failure paths, without loading or rerunning a plug-in."""
from pathlib import Path
import copy, gzip, hashlib, json, shutil, tempfile, unittest
import verify
class ReplayFailures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source=Path(__file__).resolve().parent
        cls.record=json.loads(gzip.decompress((cls.source/'campaign.json.gz').read_bytes()))
    def altered(self, edit, expected):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp); d=copy.deepcopy(self.record); edit(d)
            raw=json.dumps(d,separators=(',',':'),sort_keys=True).encode()
            compressed=gzip.compress(raw,mtime=0,compresslevel=1);(p/'campaign.json.gz').write_bytes(compressed)
            h=lambda b:hashlib.sha256(b).hexdigest()
            m={'schema':'rolling-endpoint-public-export-v1','files':{'campaign.json.gz':{'bytes':len(compressed),'sha256':h(compressed)}},'expanded_bytes':len(raw),'expanded_sha256':h(raw)}
            (p/'manifest.json').write_text(json.dumps(m))
            with self.assertRaisesRegex(ValueError,expected):verify.verify(p)
    def test_changed_grid_pcm(self):
        def edit(d):d['grid'][1]['arms']['candidate']['comparison']['segments'][2]['pcm_sha256']='0'*64
        self.altered(edit,'untimed paired equality')
    def test_duplicate_observation(self):
        def edit(d):d['runs'][1]['raw_receipt_sha256']=d['runs'][0]['raw_receipt_sha256']
        self.altered(edit,'timing receipt identity')
    def test_changed_retained_clock(self):
        def edit(d):d['runs'][2]['segments'][5]['callback_cpu_ns'][0]+=5
        self.altered(edit,'segment metrics')
    def test_inverted_promotion(self):
        def edit(d):a=d['audit'];k='primary_circuit_and_both_nontarget_guards_pass';a[k]=not a[k]
        self.altered(edit,'joint decision')
if __name__=='__main__':unittest.main()

