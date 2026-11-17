"""Actual-byte identity with tiny inert files only; no module loads."""
import copy,hashlib,json,os,pathlib,shutil,stat,struct,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent

def u64(n):return struct.pack('<Q',n)
def text(s):
    b=s.encode();return u64(len(b))+b
def eroot(r):return text(r['path'])+b''.join(u64(r[k]) for k in ('mode','uid','gid'))
def eentry(e):return text(e['path'])+b''.join(u64(e[k]) for k in ('directory','mode','uid','gid','size'))+text(e['sha256'])
def encode(m):
    out=text('cs80-native-expected-v1')+text(m['arm'])+eroot(m['bundle'])+text(m['module'])+b''.join(u64(x) for x in m['caps'])
    out+=u64(len(m['entries']))+b''.join(eentry(e) for e in m['entries'])
    out+=u64(len(m['roots']))+b''.join(eroot(r) for r in m['roots'])
    out+=u64(len(m['inputs']))+b''.join(text(i['name'])+u64(i['root'])+eentry(i['file']) for i in m['inputs'])
    return out

def record(path,relative):
    s=path.stat();d=stat.S_ISDIR(s.st_mode)
    return dict(path=relative,directory=int(d),mode=stat.S_IMODE(s.st_mode),uid=s.st_uid,gid=s.st_gid,size=0 if d else s.st_size,sha256='' if d else hashlib.sha256(path.read_bytes()).hexdigest())
def rootrecord(path):return {k:v for k,v in record(path,str(path)).items() if k in ('path','mode','uid','gid')}
def write_manifest(path,m):
    def r(x):return ' '.join([json.dumps(x['path']),*(str(x[k]) for k in ('mode','uid','gid'))])
    def e(x):return ' '.join([json.dumps(x['path']),*(str(x[k]) for k in ('directory','mode','uid','gid','size')),json.dumps(x['sha256'])])
    lines=[json.dumps(m['arm']),r(m['bundle']),json.dumps(m['module']),' '.join(map(str,m['caps'])),str(len(m['entries'])),*(e(x) for x in m['entries']),str(len(m['roots'])),*(r(x) for x in m['roots']),str(len(m['inputs'])),*(' '.join([json.dumps(x['name']),str(x['root']),e(x['file'])]) for x in m['inputs'])]
    path.write_text('\n'.join(lines)+'\n')

class NativeIdentityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('g++')
        if not compiler:raise unittest.SkipTest('Native identity requires C++ compiler and official OpenSSL development headers')
        cls.build=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.build.name)/'fixture'
        result=subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT),str(ROOT/'native_identity_fixture.cpp'),'-lcrypto','-Wl,--wrap=read','-Wl,--wrap=EVP_DigestUpdate','-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.build.cleanup()
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.base=pathlib.Path(self.temp.name)
        self.bundle=self.base/'bundle';self.bundle.mkdir();(self.bundle/'module.bin').write_bytes(b'hello module\0');(self.bundle/'empty').mkdir();(self.bundle/'data').mkdir();(self.bundle/'data'/'resource').write_bytes(b'resource')
        self.runtime=self.base/'runtime';self.runtime.mkdir();(self.runtime/'lib').write_bytes(b'inert runtime')
        self.m=dict(arm='stock',bundle=rootrecord(self.bundle),module='module.bin',caps=[32,16,4096,4096],entries=[record(self.bundle,'.')]+[record(p,p.relative_to(self.bundle).as_posix()) for p in sorted(self.bundle.rglob('*'))],roots=[rootrecord(self.runtime)],inputs=[dict(name='crypto',root=0,file=record(self.runtime/'lib','lib'))])
    def run_case(self,action='check',ok=False):
        path=self.base/'expected';write_manifest(path,self.m)
        r=subprocess.run([str(self.exe),str(path),action],capture_output=True,text=True,timeout=5)
        self.assertEqual(r.returncode,0 if ok else 1,r.stderr)
        return r
    def test_python_golden_and_deep_copy(self):
        r=self.run_case('caller-mutate',True);self.assertEqual(r.stdout.splitlines()[0],hashlib.sha256(encode(self.m)).hexdigest())
        inode=b''
        def note(label,p):
            s=p.stat();return text(label)+u64(s.st_dev)+u64(s.st_ino)
        def walk(p,rel):
            out=note('bundle:'+rel,p)
            for child in sorted(p.iterdir()):
                r=child.name if rel=='.' else rel+'/'+child.name
                out+=walk(child,r) if child.is_dir() else note('bundle:'+r,child)
            return out
        inode=walk(self.bundle,'.')+note('root:'+str(self.runtime),self.runtime)+note('input:crypto',self.runtime/'lib')
        expected=text('cs80-native-verified-v1')+u64(len(encode(self.m)))+encode(self.m)+u64(len(inode))+inode
        self.assertEqual(r.stdout.splitlines()[1],hashlib.sha256(expected).hexdigest())
    def test_punctuation_path(self):
        p=self.bundle/'!resource';p.write_bytes(b'punctuation');self.m['entries'].insert(1,record(p,p.name));self.run_case(ok=True)
    def test_content(self):(self.bundle/'module.bin').write_bytes(b'wrong module\0');self.run_case()
    def test_size(self):(self.bundle/'module.bin').write_bytes(b'x');self.run_case()
    def test_mode(self):(self.bundle/'module.bin').chmod(0o600);self.run_case()
    def test_owner(self):self.m['entries'][-1]['uid']+=1;self.run_case()
    def test_extra(self):(self.bundle/'extra').write_bytes(b'x');self.run_case()
    def test_missing(self):(self.bundle/'data'/'resource').unlink();self.run_case()
    def test_symlink(self):(self.bundle/'module.bin').unlink();(self.bundle/'module.bin').symlink_to(self.runtime/'lib');self.run_case()
    def test_hardlink(self):os.link(self.bundle/'module.bin',self.base/'alias');self.run_case()
    def test_ancestor_symlink(self):
        alias=self.base/'alias';alias.symlink_to(self.bundle,target_is_directory=True);self.m['bundle']['path']=str(alias);self.run_case()
    def test_runtime_root_symlink(self):
        alias=self.base/'alias';alias.symlink_to(self.runtime,target_is_directory=True);self.m['roots'][0]['path']=str(alias);self.run_case()
    def test_runtime_nested_symlink(self):
        (self.runtime/'alias').symlink_to(self.bundle,target_is_directory=True);self.m['inputs'][0]['file']['path']='alias/module.bin';self.run_case()
    def test_runtime_change(self):(self.runtime/'lib').write_bytes(b'wrong runtime');self.run_case()
    def test_root_mode(self):self.m['bundle']['mode']^=0o100;self.run_case()
    def test_root_missing(self):self.m['entries']=self.m['entries'][1:];self.run_case()
    def test_module_missing(self):self.m['module']='missing';self.run_case()
    def test_duplicate(self):self.m['entries'].append(copy.deepcopy(self.m['entries'][-1]));self.run_case()
    def test_input_alias(self):self.m['inputs'].append(copy.deepcopy(self.m['inputs'][0]));self.m['inputs'][-1]['name']='other';self.run_case()
    def test_overlap(self):self.m['roots'][0]=rootrecord(self.bundle/'data');self.run_case()
    def test_traversal(self):self.m['module']='../module.bin';self.run_case()
    def test_count_cap(self):self.m['caps'][0]=4;self.m['caps'][1]=4;self.run_case()
    def test_file_cap(self):self.m['caps'][1]=1;self.run_case()
    def test_raw_cap(self):self.m['caps'][2]=1;self.run_case()
    def test_overflow(self):self.m['entries'][-1]['size']=2**64-1;self.m['caps'][2]=2**64-1;self.run_case()
    def test_boundary(self):
        self.m['caps']=[len(self.m['entries'])+1,3,sum(e['size'] for e in self.m['entries'])+self.m['inputs'][0]['file']['size'],4096];self.run_case(ok=True)
    def test_unexpected_count_cap(self):
        self.m['caps'][0]=len(self.m['entries'])+1;self.m['caps'][1]=3
        for n in range(12):(self.bundle/('extra'+str(n))).write_bytes(b'x')
        self.run_case()
    def test_special(self):os.mkfifo(self.bundle/'pipe');self.run_case()
    def test_special_mode(self):(self.bundle/'module.bin').chmod(0o4644);self.run_case()

for action in ('grow','truncate','evp-fail','reverify-content','reverify-mode','reverify-extra','reverify-replace'):
    setattr(NativeIdentityTests,'test_'+action.replace('-','_'),lambda self,a=action:(self.run_case(a),None)[1])
if __name__=='__main__':unittest.main()
