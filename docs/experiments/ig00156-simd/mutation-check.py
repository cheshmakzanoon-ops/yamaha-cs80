#!/usr/bin/env python3
"""Two deliberate solver mistakes must fail the exact stage test on an AVX2 host."""
import argparse,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('output',type=pathlib.Path);p.add_argument('--compiler',default='c++');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];a.output.mkdir()
source=(root/'src/core/Ig00156Batch.cpp').read_text()
mutations={'lost_history':('scatter(&Ig00156Stage::zIn,2.0*vin-zIn);','scatter(&Ig00156Stage::zIn,zIn);'),
 'unmasked_stop':('active=_mm256_andnot_pd(less(absolute(f),1.0e-7).x,active.x);','/* deliberately ignore per-lane convergence */')}
for name,(before,after) in mutations.items():
 if source.count(before)!=1:raise SystemExit('Source changed: review mutation anchor '+name)
 cpp=a.output/(name+'.cpp');cpp.write_text(source.replace(before,after));binary=a.output/name
 subprocess.run([a.compiler,'-O2','-std=c++20','-ffp-contract=off','-I'+str(root/'src'),'-I'+str(root/'src/core'),str(root/'tests/Ig00156BatchTests.cpp'),str(cpp),'-o',str(binary)],check=True)
 result=subprocess.run([str(binary)],capture_output=True,text=True);(a.output/(name+'.log')).write_text(result.stdout+result.stderr)
 if result.returncode!=1:raise SystemExit('Mutation not detected (requires enabled AVX2): '+name)
 print(name,result.stdout.strip())
