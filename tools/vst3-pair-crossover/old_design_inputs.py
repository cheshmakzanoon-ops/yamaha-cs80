#!/usr/bin/env python3
"""Offline descriptive design inputs from the FAILED held stock comparator; does not re-gate it."""
import csv
import gzip
import json
import math
from pathlib import Path
import statistics as st
from crossover import TC
D=Path(__file__).resolve().parents[1]/'vst3-target-qualification/cloud-diagnostic'

def summary():
    twins={}
    with gzip.open(D/'clocks.csv.gz','rt') as f:
        for r in csv.DictReader(f):
            if (r['slot'].startswith('triplet-') and r['role'] in ('S0','S1') and
                int(r['cpu_end_ns'])>int(r['cpu_start_ns'])):
                twins.setdefault((r['slot'],r['stripe'],r['global_block']),{})[r['role']]=r
    diff=[];individual=[]
    for pair in twins.values():
        a,b=pair['S0'],pair['S1'];diff.append(abs(int(a['wall_start_ns'])-int(b['wall_start_ns']))/1e6)
        ns=lambda x:int(x['cpu_end_ns'])-int(x['cpu_start_ns'])
        individual.append(math.log(ns(b)/ns(a)))
    assert len(diff)==6144
    diff.sort()
    slots=json.loads((D/'summary.json').read_text())['triplets']
    assert len(slots)==12
    sd=st.stdev(math.log(s['cpu_ns']['S1']/s['cpu_ns']['S0']) for s in slots)
    return {'old_matched_twin_callbacks':len(diff),'wall_start_gap_ms_median':st.median(diff),
        'wall_start_gap_ms_empirical_p90':diff[int(.9*len(diff))],'individual_log_ratio_sd':st.stdev(individual),
        'old_fresh_triplet_log_ratio_sd':sd,'forecast_only_95pct_halfwidth_12_epochs_four_strata_percent':
        {f'within_epoch_correlation_{rho}':100*math.expm1(TC*sd*math.sqrt((1+3*rho)/48)) for rho in (0,1)},
        'old_gate_retested':False,'new_benchmarks':0}
if __name__=='__main__': print(json.dumps(summary(),indent=2))
