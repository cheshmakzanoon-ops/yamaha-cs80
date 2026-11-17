"""Synthetic fail-closed checks. Never compiles or executes a synth."""
import json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
SCRIPT=Path(__file__).with_name('orchestrate.py')
class Guards(unittest.TestCase):
    def test_render_timeout_forwarding_and_frozen_tail_bound(self):
        import ast
        tree=ast.parse(SCRIPT.read_text())
        render=next(n for n in ast.walk(tree) if isinstance(n,ast.FunctionDef) and n.name=='render')
        self.assertEqual(render.args.kwonlyargs[0].arg,'limit')
        self.assertEqual(ast.literal_eval(render.args.kw_defaults[0]),600)
        run=next(n for n in ast.walk(render) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='run')
        self.assertTrue(any(k.arg=='limit' and isinstance(k.value,ast.Name) and k.value.id=='limit' for k in run.keywords))
        tails=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='render' and isinstance(n.args[0],ast.JoinedStr) and any(isinstance(v,ast.Constant) and v.value=='tail-' for v in n.args[0].values)]
        self.assertEqual(len(tails),1)
        self.assertEqual(next(ast.literal_eval(k.value) for k in tails[0].keywords if k.arg=='limit'),120)

    def test_normal_prototype_flag_vectors(self):
        from orchestrate import compile_flags
        normal=['-std=gnu++20','-O3','-DNDEBUG','-fPIC']
        for unit in ['Engine','FactoryPresets','driver','structural','capture','followup',None]:
            self.assertEqual(compile_flags(unit),normal)
        self.assertEqual(compile_flags('Ig00156Batch'),normal+['-ffp-contract=off'])
        # Returned vectors are independent; no mutable shared flag additions.
        compile_flags().append('-ffast-math')
        self.assertEqual(compile_flags(),normal)

    def launch(self,out,env):
        return subprocess.run([sys.executable,str(SCRIPT),'--repo',str(out.parent),'--revision','unused','--out',str(out)],env=env,capture_output=True)
    def test_missing_attempt_terminal(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'new';env=dict(os.environ);env.pop('GITHUB_RUN_ATTEMPT',None)
            result=self.launch(p,env);self.assertEqual(result.returncode,1)
            terminal=json.loads((p/'terminal.json').read_text());self.assertEqual(terminal['reason'],'first attempt only');self.assertEqual(list((p/'commands').iterdir()),[])
    def test_existing_work_never_reused(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'existing';p.mkdir();(p/'sentinel').write_text('preserve')
            self.assertNotEqual(self.launch(p,dict(os.environ)).returncode,0)
            self.assertEqual([x.name for x in p.iterdir()],['sentinel'])
    def test_second_attempt_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'new';env={**os.environ,'GITHUB_RUN_ATTEMPT':'2','GITHUB_RUN_NUMBER':'1'}
            self.assertEqual(self.launch(p,env).returncode,1);self.assertEqual(json.loads((p/'terminal.json').read_text())['reason'],'first attempt only')
if __name__=='__main__':unittest.main()
