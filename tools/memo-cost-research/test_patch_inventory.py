"""Real git patch inventory gate; no compile or benchmark."""
import json,pathlib,subprocess,tempfile,unittest
import run
class ActualPatch(unittest.TestCase):
 def test_real_patch_and_unexpected_paths(self):
  repo=pathlib.Path(__file__).resolve().parents[2]
  def git(*args,cwd=repo):return subprocess.check_output(['git',*map(str,args)],cwd=cwd,stderr=subprocess.STDOUT)
  with tempfile.TemporaryDirectory() as temp:
   src=pathlib.Path(temp)/'source'
   git('worktree','add','--detach',src,run.BASE)
   try:
    git('apply',run.HERE/'candidate.patch',cwd=src)
    allowed=json.loads((run.HERE/'source.json').read_text())['allowed_paths']
    self.assertEqual(run.validate_patch_paths(src,allowed),sorted(allowed))
    self.assertEqual(len(git('ls-files','--others','--exclude-standard',cwd=src).splitlines()),2)
    extra=src/'tests/UnexpectedMemoResearch.cpp';extra.write_text('// unexpected\n')
    with self.assertRaises(AssertionError):run.validate_patch_paths(src,allowed)
    extra.unlink()
    with self.assertRaises(AssertionError):run.validate_patch_paths(src,allowed[:-1])
    with (src/'src/core/FactoryPresets.cpp').open('a') as f:f.write('\n// unexpected tracked modification\n')
    with self.assertRaises(AssertionError):run.validate_patch_paths(src,allowed)
   finally:git('worktree','remove','--force',src)
if __name__=='__main__':unittest.main()
