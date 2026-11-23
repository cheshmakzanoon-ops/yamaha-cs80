#!/usr/bin/env python3
"""Summarize exact downloaded CTest/pluginval evidence; never qualify a release.

Use --inputs INPUTS.json --out NEW_REPORT.json. Paths are explicit, relative to
INPUTS.json unless absolute. No archive is extracted. See a45-review.md.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import stat
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path, PurePosixPath

TARGETS = ('linux-x86_64', 'windows-x86_64', 'macos-arm64', 'macos-x86_64')
LANES = {
 'linux-x86_64': [
  ('headless', 'Validate VST3 headless', 'pluginval-headless.log', 'cs80-headless', False, None),
  ('gui', 'Validate VST3 GUI under Xvfb', 'pluginval-gui.log', 'cs80-gui', True, None),
  ('gui-seed', 'Validate GUI state regression seed 0x2da72db', 'pluginval-gui-seed-regression.log', 'cs80-gui-seed-regression', True, '0x2da72db')],
 'windows-x86_64': [
  ('headless', 'Validate VST3 headless', 'pluginval-windows.log', 'cs80-windows', False, None),
  ('headless-seed', 'Validate state regression seed 0x5037a0c', 'pluginval-windows-seed-regression.log', 'cs80-windows-seed-regression', False, '0x5037a0c')],
}
for _target in ('macos-arm64', 'macos-x86_64'):
 LANES[_target] = [
  ('vst3', 'Validate VST3 with pluginval', 'pluginval-vst3.log', 'cs80-vst3', False, None),
  ('au', 'Validate AU with pluginval', 'pluginval-au.log', 'cs80-au', False, None),
  ('au-gui', 'Validate AU with pluginval GUI', 'pluginval-au-gui.log', 'cs80-au-gui', True, None),
  ('au-gui-seed', 'Validate AU GUI fixed-seed regression', 'pluginval-au-seed-regression.log', 'cs80-au-seed-regression', True, '0x2b4eef5')]
MAX_FILE = 64 * 1024 * 1024
MAX_ARCHIVE_EXPANDED = 256 * 1024 * 1024


def record(data, path):
 return {'path': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def decode(data):
 return data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig').replace('\r\n', '\n')


class Evidence:
 """Read only exact member names from a ZIP or an explicitly supplied directory."""
 def __init__(self, item, base):
  self.archive = None
  self.root = None
  self.provenance = None
  if 'archive' in item:
   path = resolve(base, item['archive'])
   blob = path.read_bytes()
   if len(blob) > MAX_ARCHIVE_EXPANDED: raise ValueError('Evidence ZIP exceeds bound')
   self.provenance = record(blob, path)
   self.archive = zipfile.ZipFile(path)
   total = 0
   names = set()
   for z in self.archive.infolist():
    n = z.filename
    clean = n[:-1] if z.is_dir() else n
    parts = PurePosixPath(clean).parts
    if not clean or '\\' in n or n.startswith('/') or ':' in n or any(p in ('', '.', '..') for p in clean.split('/')) or PurePosixPath(clean).as_posix() != clean:
     raise ValueError(f'Noncanonical ZIP name: {n!r}')
    if clean in names: raise ValueError(f'Duplicate ZIP path: {clean}')
    names.add(clean)
    kind = stat.S_IFMT(z.external_attr >> 16)
    if kind not in (0, stat.S_IFREG, stat.S_IFDIR) or (kind == stat.S_IFDIR) != z.is_dir() and kind != 0:
     raise ValueError(f'Unsupported ZIP member type: {n}')
    if z.file_size > MAX_FILE: raise ValueError(f'Oversize ZIP member: {n}')
    total += z.file_size
   if total > MAX_ARCHIVE_EXPANDED: raise ValueError('Expanded ZIP exceeds bound')
   self.names = {z.filename for z in self.archive.infolist() if not z.is_dir()}
  elif 'directory' in item:
   self.root = resolve(base, item['directory'])
   if not self.root.is_dir(): raise ValueError(f'Not a directory: {self.root}')
   self.names = set()
   for p in self.root.rglob('*'):
    if p.is_symlink(): raise ValueError(f'Symlink evidence input: {p}')
    if p.is_file(): self.names.add(p.relative_to(self.root).as_posix())
  else:
   raise ValueError('Evidence entry needs archive or directory')
 def read(self, name):
  if name not in self.names: raise ValueError(f'Missing exact evidence member: {name}')
  data = self.archive.read(name) if self.archive else (self.root/name).read_bytes()
  if len(data) > MAX_FILE: raise ValueError(f'Oversize evidence member: {name}')
  return data
 def close(self):
  if self.archive: self.archive.close()


def resolve(base, value):
 p = Path(value)
 return p if p.is_absolute() else base/p


def lasttest_cases(text):
 markers = list(re.finditer(r'^\d+/\d+ Test: (.+)$', text, re.M))
 cases = {}
 for index, m in enumerate(markers):
  name = m.group(1).strip()
  chunk = text[m.end():markers[index+1].start() if index+1<len(markers) else len(text)]
  if name in cases: raise ValueError(f'Duplicate LastTest execution: {name}')
  body = re.search(r'^Output:\n-+\n(.*?)^<end of output>', chunk, re.S|re.M)
  if body is None: raise ValueError(f'Incomplete LastTest output: {name}')
  tail = chunk[body.end():]
  ending = re.search(r'^Test (Passed|Failed|Skipped)\.', tail, re.M)
  status = ending.group(1) if ending else ('Passed' if re.search(r'^Test Pass Reason:',tail,re.M) else 'Failed' if re.search(r'^Test Fail Reason:',tail,re.M) else None)
  cases[name] = {'output': body.group(1), 'lasttest_status': status}
 return cases


def python_skips(output):
 # Parse only this authoritative LastTest body. Never add JUnit copies or
 # explicit skipped-test lines to the unittest terminal-summary counts.
 summaries = []
 for match in re.finditer(r'^Ran (\d+) tests? in [^\n]+\n\s*\n?(OK|FAILED)(?: \(([^\n]*)\))?\s*$', output, re.M):
  details = match.group(3) or ''
  skip = re.search(r'(?:^|,\s*)skipped=(\d+)(?:,|$)', details)
  summaries.append({'tests_run':int(match.group(1)), 'result':match.group(2), 'skipped':int(skip.group(1)) if skip else 0, 'summary':match.group(0).strip()})
 explicit = [line for line in output.splitlines() if re.search(r'\.\.\. skipped [\"\']', line)]
 return {'total':sum(s['skipped'] for s in summaries), 'summaries':summaries, 'explicit_skip_lines':explicit,
         'scope':'Unittest terminal summaries only; explicit lines are reasons, not additional counts.'}


def native(item, base):
 target=item['target']; evidence=None
 if 'archive' in item or 'directory' in item:
  evidence=Evidence(item,base)
  jname=item.get('junit_member','ctest-results.xml'); lname=item.get('lasttest_member','Testing/Temporary/LastTest.log')
  jb=evidence.read(jname); lb=evidence.read(lname)
  provenance_name=item.get('provenance_member','ci-juce-provenance.txt')
  pb=evidence.read(provenance_name) if provenance_name in evidence.names else None
 else:
  jname=resolve(base,item['junit']); lname=resolve(base,item['lasttest'])
  jb=Path(jname).read_bytes(); lb=Path(lname).read_bytes(); pb=None
 if b'<!DOCTYPE' in jb.upper() or b'<!ENTITY' in jb.upper(): raise ValueError('DTD/entity JUnit is unsupported')
 root=ET.fromstring(jb)
 if root.tag!='testsuite': raise ValueError('Expected one CTest testsuite root')
 cases=root.findall('testcase')
 if int(root.get('tests','-1'))!=len(cases): raise ValueError('JUnit tests count does not match testcase count')
 last=lasttest_cases(decode(lb)); results=[]; skips=[]; disabled=[]; failures=[]; unknown=[]; nested=[]; names=set()
 for case in cases:
  name=case.get('name')
  if not name or name in names: raise ValueError(f'Missing/duplicate JUnit name: {name!r}')
  names.add(name)
  failure=case.find('failure'); error=case.find('error'); skipped=case.find('skipped')
  st=case.get('status','')
  if failure is not None or error is not None: result='failed'; failures.append(name)
  elif st=='disabled' or (skipped is not None and 'disabled' in skipped.get('message','').lower()): result='disabled'; disabled.append(name)
  elif skipped is not None: result='formal_skipped'; skips.append({'name':name,'status':st,'message':skipped.get('message','')})
  elif st=='run': result='passed'
  else: result='unknown'; unknown.append({'name':name,'status':st})
  output=case.findtext('system-out','')
  row={'name':name,'result':result,'seconds':float(case.get('time','0')),'junit_stdout_truncated':'[This part of the test output was removed' in output}
  if name in last:
   row['lasttest_status']=last[name]['lasttest_status']
   row['lasttest_output_sha256']=hashlib.sha256(last[name]['output'].encode()).hexdigest()
   if result=='passed' and row['lasttest_status']!='Passed': raise ValueError(f'LastTest disagrees with passing JUnit case {name}')
   ps=python_skips(last[name]['output'])
   if ps['total'] or ps['explicit_skip_lines']:
    if len(ps['explicit_skip_lines'])>ps['total']: raise ValueError(f'Unreconciled Python skip lines in {name}; inspect manually')
    nested.append({'ctest_test':name,**ps})
  elif result!='disabled': raise ValueError(f'JUnit case missing in LastTest: {name}')
  results.append(row)
 if set(last)-names: raise ValueError('LastTest contains cases absent from JUnit')
 counts={'registered':len(cases),'passed':sum(r['result']=='passed' for r in results),'failed':len(failures),'formal_skipped':len(skips),'disabled':len(disabled),'unknown':len(unknown)}
 for attr,key in [('failures','failed'),('skipped','formal_skipped'),('disabled','disabled')]:
  if int(root.get(attr,'0'))!=counts[key]: raise ValueError(f'JUnit aggregate {attr} disagrees with parsed cases')
 result={'target':target,'status':'observed_tests_green' if not failures and not unknown else 'observed_test_failure_or_unknown',
  'counts':counts,'formal_skips':skips,'disabled':disabled,'failures':failures,'unknown':unknown,
  'ctest_seconds_junit':float(root.get('time','0')),'junit_timestamp_without_assumed_timezone':root.get('timestamp'),
  'nested_python_skips':{'total':sum(x['total'] for x in nested),'suites':nested},'testcases':results,
  'inputs':{'junit':record(jb,jname),'lasttest':record(lb,lname)},
  'ctest_process_exit':None,'requires_original_successful_test_step':True,
  'scope':'CTest testcase executions, not internal assertions. Formal skips may be labelled Passed in LastTest; JUnit is authoritative.'}
 if pb is not None: result['juce_provenance']={'text':decode(pb),**record(pb,provenance_name)}
 if evidence:
  result['archive']=evidence.provenance;evidence.close()
 return result


def console_summary(data):
 text=decode(data); lines=[x.strip() for x in text.splitlines() if x.strip()]
 strictness=re.findall(r'^Strictness level:\s*(\d+)\s*$',text,re.M)
 seeds=re.findall(r'^Random seed:\s*(0x[0-9a-fA-F]+)\s*$',text,re.M)
 programs=re.findall(r'^Num programs:\s*(\d+)\s*$',text,re.M)
 success=sum(x=='SUCCESS' for x in lines)
 warnings=[x for x in lines if x.startswith('!!! WARNING:')]
 failures=[x for x in lines if 'FAILED' in x or (x.startswith('!!!') and x not in warnings)]
 editor=bool(re.search(r'^Starting tests in: pluginval / Editor\.\.\.\s*$',text,re.M))
 return {'strictness':list(map(int,strictness)),'seeds':seeds,'program_counts':list(map(int,programs)),
  'success_lines':success,'final_nonempty_line':lines[-1] if lines else None,'failure_markers':failures,'warnings':warnings,
  'editor_test_started':editor,'explicit_skips':[x for x in lines if re.search(r'\b[Ss]kipping\b',x)],
  'clean_text_success':success==1 and bool(lines) and lines[-1]=='SUCCESS' and not failures,
  'validation_start_count':len(re.findall(r'^Validation started\s*$',text,re.M))}


def validators(item,base):
 target=item['target']; e=Evidence(item,base); rows=[]
 for lane,step,console,report,gui,seed in LANES[target]:
  console_path=item.get('console_members',{}).get(lane,console)
  report_path=item.get('report_members',{}).get(lane,'pluginval-reports/'+report)
  cb=e.read(console_path); rb=e.read(report_path)
  cs=console_summary(cb); rs=console_summary(rb)
  issues=[]
  for label,s in [('console',cs),('report',rs)]:
   if not s['clean_text_success']:issues.append(label+' lacks exactly one terminal clean SUCCESS')
   if s['strictness']!=[10]:issues.append(label+' strictness is not exactly 10')
   if s['program_counts']!=[523]:issues.append(label+' program count is not exactly 523')
   if len(s['seeds'])!=1:issues.append(label+' lacks one seed')
   if seed and [x.lower() for x in s['seeds']]!=[seed]:issues.append(label+' fixed seed mismatch')
   if s['editor_test_started']!=gui:issues.append(label+' GUI test coverage differs from required lane')
  if cs['seeds']!=rs['seeds']:issues.append('Console/report seed mismatch')
  stem=console_path[:-4]
  attempts=sorted(n for n in e.names if re.fullmatch(re.escape(stem)+r'-attempt[0-9]+\.log',n))
  prior=[{'member':n,**record(e.read(n),n),'summary':console_summary(e.read(n))} for n in attempts]
  row={'lane':lane,'required_workflow_step':step,'gui_required':gui,'fixed_seed':seed,
   'console':{**record(cb,console_path),'summary':cs},'report':{**record(rb,report_path),'summary':rs},
   'preserved_failed_attempts':prior,'validation_retries':len(prior),'validation_invocations':1+len(prior),
   'text_findings':issues,'original_process_exit':None,'requires_original_step_metadata_and_exit_propagation':True}
  if target=='linux-x86_64':
   core=item.get('core_members',{}).get(lane,stem+'-core.log'); kb=e.read(core); kt=decode(kb)
   terms=[{'pid':int(m[0]),'raw_returncode':int(m[1]),'kind':m[2],'details':m[3]} for m in re.findall(r'Original validator termination: PID=(\d+); raw returncode=(-?\d+); kind=([^;\s]+); ([^\n]*)',kt)]
   row['core_log']={**record(kb,core),'original_terminations':terms}
   row['final_core_log_normal_exit_zero']=bool(terms) and terms[-1]['raw_returncode']==0 and terms[-1]['kind']=='exit'
   if not row['final_core_log_normal_exit_zero']:issues.append('No final original-child normal-zero termination in core log')
   if len(terms)!=len(prior)+1:issues.append('Core termination count differs from preserved retries plus final invocation')
   if len(prior)>2:issues.append('More than two retries')
   if attempts != [stem+f'-attempt{i}.log' for i in range(1,len(prior)+1)]:issues.append('Retry numbering is not contiguous')
   row['retry_eligibility_requires_manual_job_log_review']=bool(prior)
  elif prior:issues.append('Unexpected retries on a non-Linux lane')
  rows.append(row)
 if target=='linux-x86_64':
  vb=e.read('pluginval-version.txt'); version={'record':record(vb,'pluginval-version.txt'),'text':decode(vb).strip()}
  version_ok=version['text']=='pluginval - 1.0.4'
 else:
  vb=e.read('pluginval-download.json'); vd=json.loads(decode(vb)); version={'record':record(vb,'pluginval-download.json'),'download':vd}
  version_ok=vd.get('version')=='1.0.4' and vd.get('verified') is True
 result={'target':target,'status':'observed_logs_green_unbound' if version_ok and all(not x['text_findings'] for x in rows) else 'observed_log_failure_or_incomplete',
  'required_lanes':len(rows),'validation_invocations':sum(x['validation_invocations'] for x in rows),
  'validation_retries':sum(x['validation_retries'] for x in rows),'version':version,'version_text_expected':version_ok,
  'lanes':rows,'archive':e.provenance,'qualification':False,
  'scope':'Console/report summaries are one lane, not two invocations. Original job/step/source/attempt/payload binding remains mandatory. SUCCESS followed by a crash is not accepted by text alone.'}
 e.close();return result


def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--inputs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--require-complete',action='store_true',help='Require all four native and four validator targets')
 args=p.parse_args()
 if args.out.exists():raise ValueError('Refusing an existing --out path; use a fresh filename')
 raw=args.inputs.read_bytes();cfg=json.loads(raw);base=args.inputs.resolve().parent
 output={'schema':1,'status':'parsed_evidence_not_release_qualified','release_qualified':False,
  'input_manifest':record(raw,args.inputs),'parser':record(Path(__file__).read_bytes(),Path(__file__)),
  'caller_provenance_claims_unverified':cfg.get('provenance',{}),'native':[],'validators':[],
  'external_gates':['Authentic artifact API byte count/digest/name/run/head SHA/expiry.',
   'Producer upload and native test steps from the intended run attempt.',
   'Original validator job/step outcomes and source-pinned exit propagation.',
   'Installation payload binding to validated binaries; macOS AU before/after identity.',
   'Review all preserved failed attempts; diagnostic reruns never replace original lanes.']}
 bad=False
 for kind,fn in [('native',native),('validators',validators)]:
  seen=set()
  for item in cfg.get(kind,[]):
   t=item['target']
   if t not in TARGETS or t in seen:raise ValueError('Unknown or duplicate target '+t)
   seen.add(t);r=fn(item,base);output[kind].append(r)
   if r['status'] not in ('observed_tests_green','observed_logs_green_unbound'):bad=True
  if args.require_complete and seen!=set(TARGETS):raise ValueError(kind+' does not contain all four targets')
 output['native_totals']={k:sum(x['counts'][k] for x in output['native']) for k in ('registered','passed','failed','formal_skipped','disabled','unknown')}
 output['nested_python_skips_total']=sum(x['nested_python_skips']['total'] for x in output['native'])
 output['validator_totals']={k:sum(x[k] for x in output['validators']) for k in ('required_lanes','validation_invocations','validation_retries')}
 output['counting_scope']='CTest executions across supplied targets. Internal assertions and nested unittest skips are not added to CTest counts. Console/report copies count once.'
 output['all_requested_observations_green']=not bad
 args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(output,indent=2)+'\n')
 print(json.dumps({k:output[k] for k in ('status','native_totals','nested_python_skips_total','validator_totals','all_requested_observations_green')}))
 return 1 if bad else 0


if __name__=='__main__':
 try:sys.exit(main())
 except (ValueError,KeyError,OSError,UnicodeError,zipfile.BadZipFile,ET.ParseError) as exc:
  print('ERROR: '+str(exc),file=sys.stderr);sys.exit(2)
