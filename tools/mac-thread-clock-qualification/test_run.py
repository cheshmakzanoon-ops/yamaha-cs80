import csv, io, json, tempfile, unittest
from pathlib import Path
from run import compare, patch, inventory
class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.dirs=[Path(self.tmp.name)/str(i) for i in range(3)]
        for i,d in enumerate(self.dirs):
            d.mkdir(); (d/'metadata.json').write_text(json.dumps({'fixture':'same','thread_cpu_clock_available':i>0,'callback_cpu_requested':i==2}))
            (d/'completed.json').write_text(json.dumps({'completed':True,'cases':11,'rows':19}))
            self.put(i, {'callbacks':'16','checksum':'0.123','controller_events':'2','audio_seconds':'0.04','callback_cpu_status':'available' if i==2 else 'disabled','callback_cpu_call_count':'16' if i==2 else '0','callback_cpu_seconds':'0.001','callback_cpu_rtf':'0.025'})
    def put(self,i,r):
        with (self.dirs[i]/'results.csv').open('w') as f:
            r={**r,'isolation':'0','scenario':'','initial_selector':'0','phase':'','rate':'48000','block':'128','nonfinite_count':r.get('nonfinite_count','0')}
            w=csv.DictWriter(f,fieldnames=r); w.writeheader()
            for mode,scenario,selector,phase in sorted(inventory()):
                w.writerow({**r,'isolation':mode,'scenario':scenario,'initial_selector':selector,'phase':phase})
    def mutate(self,k,v):
        r=list(csv.DictReader(io.StringIO((self.dirs[2]/'results.csv').read_text())))[0]; r[k]=v; self.put(2,r)
    def test_public_deployment_guard(self):
        source=Path(__file__).with_name('fp.h').read_text()
        self.assertIn('#include <AvailabilityMacros.h>',source)
        self.assertIn('#if !defined(MAC_OS_X_VERSION_MIN_REQUIRED) || MAC_OS_X_VERSION_MIN_REQUIRED < 101200',source)
        self.assertNotIn('__MAC_OS_X_VERSION_MIN_REQUIRED',source)
    def test_success(self): self.assertTrue(compare(self.dirs)['qualified'])
    def test_mutations(self):
        for k,v in [('nonfinite_count','1'),('checksum','0.124'),('controller_events','3'),('callbacks','17'),('callback_cpu_status','unavailable'),('callback_cpu_seconds','nan'),('callback_cpu_rtf','0.5'),('callback_cpu_call_count','15')]:
            with self.subTest(k=k):
                original=(self.dirs[2]/'results.csv').read_text(); self.mutate(k,v)
                with self.assertRaises(ValueError): compare(self.dirs)
                (self.dirs[2]/'results.csv').write_text(original)
    def test_inventory_drop(self):
        p=self.dirs[2]/'results.csv'; p.write_text('\n'.join(p.read_text().splitlines()[:-1])+'\n')
        with self.assertRaises(ValueError): compare(self.dirs)
    def test_metadata(self):
        p=self.dirs[2]/'metadata.json'; m=json.loads(p.read_text()); m['fixture']='changed'; p.write_text(json.dumps(m))
        with self.assertRaises(ValueError): compare(self.dirs)
    def test_clock_fail(self):
        p=self.dirs[2]/'metadata.json'; m=json.loads(p.read_text()); m['thread_cpu_clock_available']=False; p.write_text(json.dumps(m))
        with self.assertRaises(ValueError): compare(self.dirs)
    def test_patch_fail_closed(self):
        with self.assertRaises(ValueError): patch('#if defined(__linux__)\n')
    def test_patch_only_guards(self):
        s='#if defined(__linux__)\n#include <time.h>\n#endif\n#if defined(__linux__)\nclock_gettime(CLOCK_THREAD_CPUTIME_ID, &time)\n#endif'
        self.assertEqual(patch(s).replace(' || defined(__APPLE__)',''),s)
if __name__=='__main__': unittest.main()
