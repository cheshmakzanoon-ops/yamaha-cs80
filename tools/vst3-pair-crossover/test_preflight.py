import copy
import csv
import io
import json
from pathlib import Path
import tempfile
import unittest

import crossover as d
import phase_q as q
import preflight as dry

class Schedule(unittest.TestCase):
    def test_wake_and_full_baton_counts(self):
        for kind,want in (("wake-two",49),("full",1840)):
            order,plans=dry.schedule(kind,0,0)
            rows=list(dry.visits(kind,order,plans,0));other=list(dry.visits(kind,order,plans,1))
            self.assertEqual(len(rows),want)
            self.assertEqual([r[:-1] for r in rows],[r[:-1] for r in other])
            self.assertEqual([int(r[0]) for r in rows],list(range(want)))
            self.assertEqual(sum(int(r[5]) for r in rows if r[3]=="1"),72 if kind=="wake-two" else 2880)
    def test_wrong_stock_refused_before_process(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);s=root/"not-stock";s.write_text("fake")
            with self.assertRaisesRegex(ValueError,"one admitted"):
                dry.execute(Path("/bin/true"),s,root/"result","full",0,0)
            self.assertFalse((root/"result").exists())
    def test_no_selective_retry(self):
        data={"schema":q.SCHEMA,"sources":{}}
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/"seal.json").write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError,"explicit SHA"):
                q.inputs(p,"0"*64)
            (p/"Q-run").mkdir()
            with self.assertRaises(FileExistsError):
                (p/"Q-run").mkdir()

def synthetic(root,scene="idle",epoch=0,cell=0):
    # Generate strictly scheduled clock endpoint integers. Never load or time a module.
    pts=[{},{}]; t=100_000; cpu=[1000,9000]
    for i,l in d.visits(d.plan(epoch,cell,scene)):
        pts[l][i]=(i,cpu[l],cpu[l]+20,t,t+22);t+=30;cpu[l]+=25
    for l in (0,1):
        p=root/f"peer-{l}"/"capture"/scene;p.mkdir(parents=True)
        def clockfile(path,rows):
            with path.open("w") as f:w=csv.writer(f,lineterminator="\n");w.writerow(("index","cpu0","cpu1","wall0","wall1"));w.writerows(rows)
        clockfile(p/"clocks.csv",[pts[l][i] for i in range(576)])
        empty=[(i,cpu[l]+5*i,cpu[l]+5*i+1,t+5*i,t+5*i+3) for i in range(128)]
        clockfile(p/"empty.csv",empty)
        (p/"resources.csv").write_text(f"wall0,wall1,voluntary_switches,involuntary_switches,minor_faults,major_faults\n90000,{t+701},2,3,1,0\n")
    return {"start":80_000,"end":t+800}

class StageIntegrity(unittest.TestCase):
    def test_dry_archive_manifest_and_plan(self):
        import tarfile, hashlib
        here=Path(__file__).resolve().parent/"zero-preflight";m=json.loads((here/"receipt.json").read_bytes())
        self.assertEqual(m["stock_sha256"],d.SHIPPING_CUTOFF);self.assertTrue(m["complete"])
        self.assertEqual(sum(x["callback_count"] for x in m["public_capture_per_worker"].values()),40404)
        with tarfile.open(here/"reference.tar.xz","r|xz") as archive:
            seen=set()
            for item in archive:
                n=item.name;self.assertFalse(n.startswith("/") or ".." in Path(n).parts)
                parts=Path(n).parts
                if len(parts)==4 and parts[:2]==("isolated","capture"):
                    data=archive.extractfile(item).read();info=m["public_capture_per_worker"][parts[2]]["artifacts"][parts[3]]
                    self.assertEqual((len(data),hashlib.sha256(data).hexdigest()),(info["bytes"],info["sha256"]));seen.add(parts[2:])
            self.assertEqual(len(seen),25)
    def test_real_not_invoked_q_raw_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);bounds=synthetic(root);obs=q.scene_clocks(root,"idle",0,0,50,bounds)
            self.assertEqual(len(obs["clocks"]),1152);self.assertEqual(obs["clocks"][0][2],1000)
            self.assertEqual(obs["clocks"][0][4],100000-50)
            self.assertEqual(sum(map(len,obs["empty_endpoints"])),256);q.verify_scene_order([obs])
            self.assertEqual(d.reduced(obs)["a_common"],512*20)
    def test_forged_clocks_or_envelope(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);bounds=synthetic(root)
            def check():return q.scene_clocks(root,"idle",0,0,50,bounds)
            with self.assertRaisesRegex(ValueError,"bounds"):
                q.scene_clocks(root,"idle",0,0,50,{"start":100001,"end":bounds["end"]})
            path=root/"peer-0/capture/idle/clocks.csv";lines=path.read_text().splitlines()
            for column,new in ((0,999),(2,1000),(3,100200),(4,99999)):
                bad=lines.copy();row=bad[1].split(",");row[column]=str(new);bad[1]=",".join(row)
                path.write_text("\n".join(bad)+"\n")
                with self.assertRaises(ValueError):check()
            path.write_text("\n".join(lines[0:-1])+"\n")
            with self.assertRaisesRegex(ValueError,"incomplete clock"):check()
    def test_cross_scene_cpu_or_wall_reset_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);bounds=synthetic(root);one=q.scene_clocks(root,"idle",0,0,50,bounds)
            with self.assertRaisesRegex(ValueError,"cross-scene"):q.verify_scene_order([one,copy.deepcopy(one)])

if __name__=="__main__":unittest.main()
