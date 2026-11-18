import csv
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path
import verify as v

class ReplayTests(unittest.TestCase):
    def test_archived_result(self):
        d=v.replay()
        self.assertEqual(d["completed"],128)
        self.assertEqual(set(d["cases"]),{"factory-held","factory-moving","panel-moving","pure-held"})

    def altered(self,change,error):
        with tempfile.TemporaryDirectory() as d:
            dst=Path(d)
            for n in ("admission.json","qualification.json","candidate.patch","cost.cpp","raw.csv","result.json"):
                shutil.copyfile(v.BASE/n,dst/n)
            raw=dst/"raw.csv";r=list(csv.DictReader(raw.read_text().splitlines()))
            r=change(r);out=io.StringIO(newline="");w=csv.DictWriter(out,fieldnames=list(r[0].keys()));w.writeheader();w.writerows(r)
            b=out.getvalue().encode();raw.write_bytes(b)
            rp=dst/"result.json"; result=json.loads(rp.read_text());result["raw_sha256"]=v.sha(b);rp.write_text(json.dumps(result))
            with self.assertRaisesRegex(ValueError,error):v.replay(dst)
    def test_missing_slot(self):self.altered(lambda r:r[:-1],"incomplete")
    def test_wrong_arm(self):
        def edit(r):r[0]["arm"]="candidate";return r
        self.altered(edit,"slot")
    def test_audio_fingerprint(self):
        def edit(r):r[0]["pcm_fnv64"]="0000000000000000";return r
        self.altered(edit,"signal")
    def test_wrong_cpu_sum(self):
        def edit(r):r[32]["render_cpu_ns"]=str(int(r[32]["render_cpu_ns"])*3);return r
        self.altered(edit,"statistic")

if __name__=="__main__":unittest.main()
