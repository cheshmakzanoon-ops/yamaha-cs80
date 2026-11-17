"""Optional pinned-header syntax test only; never loads a plugin or runs SDK code."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
class NativeSdkSyntaxTests(unittest.TestCase):
    def test_pinned_sdk_adapter_syntax(self):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:self.skipTest('Set CS80_OBSERVER_SDK to the pinned header tree for compile-only SDK check')
        sdk=pathlib.Path(value)
        manifest=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())
        for name,digest in manifest['headers'].items():
            path=sdk/name
            self.assertFalse(path.is_symlink())
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),digest,name)
        compiler=shutil.which('g++')
        self.assertIsNotNone(compiler,'C++ compiler required for selected SDK validation')
        with tempfile.TemporaryDirectory() as directory:
            source=pathlib.Path(directory)/'syntax.cpp';source.write_text('#include "native_observer_sdk.hpp"\n')
            result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT),'-I',str(sdk),'-fsyntax-only',str(source)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__':unittest.main()
