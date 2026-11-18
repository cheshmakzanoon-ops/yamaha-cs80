#!/usr/bin/env python3
"""Materialize authenticated replay inputs in a NEW directory. Does not compile/run DSP."""
import argparse,sys
from pathlib import Path
sys.dont_write_bytecode=True
from verify import verify_package,read_tree,reconstruct,apply_patch,need,sha
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stock-src',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
files,docs=verify_package();stock=read_tree(a.stock_src);candidate=reconstruct(stock,docs['source-inventory.json'],files['candidate.patch']);observed=apply_patch(candidate,files['observer.patch']);need(sha(observed['core/EngineFilterEight.cpp'])==docs['provenance.json']['observer_source_sha256'],'observed source');need(not a.out.exists(),'output must not exist');a.out.mkdir(parents=True)
for name,tree in [('stock',stock),('candidate',candidate)]:
 for rel,data in tree.items():
  out=a.out/name/'src'/rel;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
(a.out/'EngineFilterEight-observed.cpp').write_bytes(observed['core/EngineFilterEight.cpp'])
for rel,data in files.items():
 if rel.startswith('fixtures/'):
  out=a.out/rel;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
for name in ['source_factory_benchmark.cpp','CallbackCpuMeasurement.h']:
 out=a.out/'fixtures/fixture'/name;out.parent.mkdir(exist_ok=True);out.write_bytes(stock['tools/'+name])
print('Prepared exact source and portable fixture inputs only. No compiler, renderer or clock ran.')
