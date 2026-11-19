from collections import Counter
import tempfile
import unittest
from pathlib import Path

import run as q


def sample(mode="measure", index=0):
    """Fake endpoints for parser controls only, never used as CPU evidence."""
    m, orders = q.schedule(index)
    raw, system = [], []
    w, c = 1_000_000, 3_000_000
    for s, order in enumerate(orders):
        for p, n in enumerate("0" if mode=="reference" else order):
            role, lane = q.ROLES[int(n)], (0 if mode=="reference" else int(m[int(n)]))
            prefix = [s, p, role, lane]
            start=w
            for j in range(17):
                w+=2; c+=1
                clock = (w,c,c+10,w+11) if mode=="measure" and (j<16 or n=="2") else (0,0,0,0)
                raw.append(dict(zip(q.RAW_FIELDS,map(str,[*prefix,j,1024+17*s+j,*clock]))))
                c+=11; w+=12
            system.append(dict(zip(q.OS_FIELDS,map(str,[*prefix,start,w,0,1,0,0]))))
            w+=2
    return raw,system


class Controls(unittest.TestCase):
    def test_design_balanced_by_stripe_and_physical_alloc_order(self):
        for first in (0,6):
            group=list(map(q.schedule,range(first,first+6)))
            self.assertEqual({m for m,_ in group},set(q.PERMS))
            for s in range(q.STRIPES):
                self.assertEqual(Counter(orders[s] for _,orders in group),Counter(q.PERMS))
        for invalid in (-1,12,True,0.0):
            with self.assertRaises(ValueError):
                q.schedule(invalid)

    def test_measured_work(self):
        for i in range(12):
            raw,system=sample(index=i)
            row=q.validate_raw(raw,system,"measure",i)
            self.assertEqual(row["cpu_ns"],{"S0":5120,"S1":5120,"S+":5440})
            self.assertEqual((row["positive_common_cpu_ns"],row["positive_extra_cpu_ns"]),(5120,320))
            self.assertEqual(row["measured_calls"],{"S0":512,"S1":512,"S+":544})
            self.assertEqual(row["index"],i)

    def test_untimed_keeps_work_without_callback_clocks(self):
        for mode in ("reference","untimed"):
            raw,system=sample(mode)
            self.assertEqual(sum(q.validate_raw(raw,system,mode,0)["cpu_ns"].values()),0)
            raw[0]["cpu_start_ns"]="1"
            with self.assertRaisesRegex(ValueError,"untimed"):
                q.validate_raw(raw,system,mode,0)

    def test_rejects_bad_raw(self):
        def mutate(test):
            raw,os=sample(); test(raw,os)
            with self.assertRaises(ValueError):
                q.validate_raw(raw,os,"measure",0)
        cases=[
            lambda r,o:r.pop(),
            lambda r,o:o.pop(),
            lambda r,o:r[1].update(r[0]),
            lambda r,o:r[0].update(global_block="1025"),
            lambda r,o:r[0].update(role="C"),
            lambda r,o:o[0].update(lane="99"),
            lambda r,o:o[1].update(wall_start_ns=o[0]["wall_start_ns"]),
            lambda r,o:r[0].update(wall_start_ns="1"),
            lambda r,o:r[0].update(cpu_end_ns=r[0]["cpu_start_ns"]),
            lambda r,o:r[1].update(cpu_start_ns=r[0]["cpu_start_ns"]),
            lambda r,o:r[0].update(cpu_start_ns="01"),
            lambda r,o:r[0].update(cpu_start_ns="1e3"),
            lambda r,o:next(x for x in r if x["role"]=="S+" and x["index"]=="16").update(cpu_start_ns="0"),
            lambda r,o:next(x for x in r if x["role"]=="S0" and x["index"]=="16").update(cpu_start_ns="1"),
            lambda r,o:o[0].update(major_faults="-1"),
        ]
        for i,case in enumerate(cases):
            with self.subTest(i):
                mutate(case)

    def test_csv_headers_width_and_values(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"a.csv"; fields=("a","b")
            for payload in ("a,b,c\n1,2,3\n","a,b\n1\n","a,b\n1,2,3\n"):
                path.write_text(payload)
                with self.assertRaises(ValueError):
                    q.rows(path,fields)
            path.write_text("a,b\n1,2\n")
            self.assertEqual(q.rows(path,fields),[{"a":"1","b":"2"}])

    @unittest.skipUnless(q.sys.platform.startswith("linux"),"Linux collector only")
    def test_virtual_machine_can_smoke_but_not_collect(self):
        info={"architecture":"x86_64","cpu":3,"allowed_cpu_list":[3],"virtualization_evidence":["cpu hypervisor flag"]}
        q.check_target(info,False,False)
        q.check_target(info,True,False,cloud=True)
        with self.assertRaisesRegex(ValueError,"cannot be a dedicated"):
            q.check_target(info,True,True,cloud=True)
        with self.assertRaisesRegex(ValueError,"only for collection"):
            q.check_target(info,False,False,cloud=True)
        with self.assertRaisesRegex(ValueError,"virtualized"):
            q.check_target(info,True,True)
        info["virtualization_evidence"]=[]
        with self.assertRaisesRegex(ValueError,"dedicated"):
            q.check_target(info,True,False)
        q.check_target(info,True,True)
        info["allowed_cpu_list"]=[]
        with self.assertRaisesRegex(ValueError,"affinity"):
            q.check_target(info,False,False)

    def test_short_artifacts_not_evidence(self):
        total=q.WARM+q.STRIPES*q.WIDTH+1
        with tempfile.TemporaryDirectory() as d:
            p=Path(d); lane=p/"lane0"; lane.mkdir()
            for file in q.FILES:
                (lane/file).write_bytes(b"some bytes")
            (p/"controls.csv").write_bytes(b"controls")
            with self.assertRaisesRegex(ValueError,"PCM"):
                q.observations(p,1)
            (lane/"pcm.bin").write_bytes(b"a"*(total*512))
            with self.assertRaisesRegex(ValueError,"trace"):
                q.observations(p,1)
            (lane/"callbacks.bin").write_bytes(b"a"*(total*68))
            (lane/"initial.state").write_bytes(b"")
            with self.assertRaisesRegex(ValueError,"empty"):
                q.observations(p,1)
            (lane/"initial.state").write_bytes(b"x")
            original=q.observations(p,1)[0]
            (lane/"initial.state").write_bytes(b"y")
            self.assertNotEqual(original,q.observations(p,1)[0])


if __name__=="__main__":
    unittest.main()
