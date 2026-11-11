#!/usr/bin/env python3
"""Extend a freshly prepared adapter evidence directory with processor evidence."""
import argparse, pathlib, subprocess, shutil, sys
if sys.flags.optimize: raise SystemExit('REFUSE optimized Python')
p=argparse.ArgumentParser()
p.add_argument('--repo',required=True,type=pathlib.Path);p.add_argument('--upstream',required=True,type=pathlib.Path)
p.add_argument('--evidence',required=True,type=pathlib.Path)
a=p.parse_args();here=pathlib.Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here/'prepare.py'),'--repo',str(a.repo),'--upstream',str(a.upstream),'--evidence',str(a.evidence)],check=True)
for name in ['processor.cpp','prepare_processor.py','build_processor.py','run_processor.py']:
 shutil.copyfile(here/name,a.evidence/name)
