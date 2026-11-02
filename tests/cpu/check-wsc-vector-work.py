#!/usr/bin/env python3
"""Pair the exact runtime admission oracle with real four-double arithmetic.

This is a bounded experiment witness, not CPU qualification. The runtime oracle
must separately pass; instruction presence alone cannot prove execution or gain.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


def inspect(binary, objdump):
    result=subprocess.run([objdump,'-d','-C',str(binary)],text=True,
                          stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError('objdump failed: '+result.stderr.strip())
    lines=result.stdout.splitlines()
    body=[]
    collecting=False
    for line in lines:
        symbol=re.match(r'^[0-9a-fA-F]+ <(.+)>:$',line)
        if symbol:
            collecting='cs80::ReconstructedOscillator::advanceTickFourAvx2(' in symbol[1]
        elif collecting:
            body.append(line)
    instructions={name:sum(bool(re.search(r'\b'+name+r'\b.*%ymm',line)) for line in body)
                  for name in ('vaddpd','vsubpd','vmulpd','vdivpd')}
    calls=sum(bool(re.search(r'\bcall\w*\b.*<cs80::ReconstructedOscillator::advanceTickFourAvx2\(',line))
              for line in lines)
    ok=bool(body) and calls>0 and all(instructions.values())
    return {'binary':str(binary),'kernel_found':bool(body),'kernel_call_sites':calls,
            'four_double_arithmetic_instructions':instructions,'passed':ok,
            'claim':'compiled arithmetic plus separately checked runtime admission; no performance claim'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    parser.add_argument('--objdump',default='objdump')
    args=parser.parse_args()
    try:
        result=inspect(args.binary,args.objdump)
        print(json.dumps(result,indent=2))
        if not result['passed']:
            print('FAIL WSC real four-double vector arithmetic missing',file=sys.stderr)
            return 1
        return 0
    except (OSError,RuntimeError) as error:
        print('WSC vector work inspection error: '+str(error),file=sys.stderr)
        return 2

if __name__=='__main__':
    sys.exit(main())
