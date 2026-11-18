#!/usr/bin/env python3
"""Untimed negative tests; frozen candidate sources are never modified."""
import argparse,hashlib,json,pathlib,resource,subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('candidate_checkout',type=pathlib.Path)
parser.add_argument('output_directory',type=pathlib.Path)
args=parser.parse_args()
candidate=args.candidate_checkout.resolve()
root=args.output_directory.resolve(); root.mkdir(parents=True,exist_ok=False)
component=pathlib.Path(__file__).with_name('component.cpp')
source=candidate/'src/core/Ig00156Batch.cpp'
text=source.read_text()
assert hashlib.sha256(source.read_bytes()).hexdigest() == '51a5786f86daf150c0cd158b761ed941160445dc80501fbe53bcdfb519539c0f', 'not the frozen V2 source'
mutations={
 'drop-sticky-eligibility': (' || (csr & memo.fpFlags) != memo.fpFlags',''),
 'allow-raw-q-only-donor': ('if (cb == previousCutoff || !ordinaryPositive(cb))','if (!ordinaryPositive(cb))'),
 'corrupt-shared-kHalf-one-ulp': ('s.kHalf = memo.coefficient[0];','s.kHalf = std::bit_cast<double>(std::bit_cast<std::uint64_t>(memo.coefficient[0]) + 1U);'),
 'drop-derived-identity': ('''        for (std::size_t i = 0; i < derived.size(); ++i)
            if (std::bit_cast<std::uint64_t>(derived[i])
                != std::bit_cast<std::uint64_t>(memo.derived[i]))
                return false;''','')
}
flags=['-O3','-DNDEBUG','-std=gnu++20','-fPIC','-Wall','-Wextra','-Wpedantic']
def limit():resource.setrlimit(resource.RLIMIT_CPU,(20,20))
def run(cmd,path):
 with path.open('wb') as out:
  p=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,timeout=30,preexec_fn=limit)
 return p.returncode
results=[]
for name,(before,after) in mutations.items():
 assert text.count(before)==1,(name,text.count(before))
 directory=root/'candidate-v2-mutations'/name;directory.mkdir(parents=True,exist_ok=True)
 changed=directory/'Ig00156Batch.cpp';changed.write_text(text.replace(before,after))
 obj=directory/'Ig00156Batch.o';exe=directory/'component'
 build=['c++',*flags,'-ffp-contract=off','-I'+str(candidate/'src/core'),'-c',str(changed),'-o',str(obj)]
 link=['c++',*flags,'-I'+str(candidate/'src'),str(component),str(obj),'-o',str(exe)]
 buildcode=run(build,directory/'build.log')
 linkcode=run(link,directory/'link.log') if buildcode==0 else None
 testcode=run([str(exe)],directory/'test.log') if linkcode==0 else None
 result={'name':name,'source_sha256':hashlib.sha256(changed.read_bytes()).hexdigest(),
         'compile_command':build,'link_command':link,'compile_exit':buildcode,'link_exit':linkcode,'test_exit':testcode,
         'negative_control_detected':testcode==1,'test_log':str((directory/'test.log').relative_to(root))}
 if testcode is not None:result['last_line']=(directory/'test.log').read_text().strip().splitlines()[-1]
 results.append(result)
 print(name,buildcode,linkcode,testcode,result.get('last_line'))
report={'frozen_source':'src/core/Ig00156Batch.cpp','frozen_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'constraints':{'untimed':True,'one_process_at_a_time':True,'per_process_cpu_limit_seconds':20,'per_process_timeout_seconds':30},
        'all_four_negative_controls_detected':all(x['negative_control_detected'] for x in results),'results':results}
(root/'candidate-v2-mutations.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['all_four_negative_controls_detected'] else 1)
