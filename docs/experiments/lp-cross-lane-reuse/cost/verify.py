"""Replay measured CPU evidence. No compilation, renderer, or plugin is invoked."""
from pathlib import Path
import hashlib,json,sys
sys.dont_write_bytecode=True
from protocol import schedule,analyze,self_test,CASES,PRIMARY
def need(ok,reason):
 if not ok:raise ValueError(reason)
def load(p):return json.loads(p.read_text())
def verify(rows,expected,receipt,raw_hash=None):
 need(receipt['primary']==PRIMARY and receipt['cases']==CASES,'endpoint change')
 need(receipt['plan_sha256']==expected['plan_sha256'],'plan mismatch')
 need(receipt['schedule']=={'stock_aa_reversal_pairs':2,'comparison_reversal_pairs':6,'processes_per_pair_and_case':8,'cases':4},'design change')
 need(set(receipt['expected_identity'])==set(CASES),'incomplete fixture identities')
 need(receipt['diagnostic_late_hits']=={'factory-moving':838176,'factory-held':0,'panel-moving':0,'pure-held':0},'work/efficacy receipt changed')
 if raw_hash is not None:need(expected['raw_sha256']==raw_hash,'raw hash mismatch')
 slots=schedule();need(len(rows)==len(slots)==256,'incomplete/extended screen')
 seen=set()
 for sl,row in zip(slots,rows):
  need(set(row)==set(sl)|{'cpu_ns','empty_cpu_ns','cpu_start','cpu_end','identity'},'observation fields')
  need(all(row[k]==v for k,v in sl.items()) and row['index'] not in seen,'order/arm mismatch');seen.add(row['index'])
  need(type(row['cpu_ns']) is int and type(row['empty_cpu_ns']) is int and row['cpu_ns']>row['empty_cpu_ns']>0,'clock')
  need(row['identity']==receipt['expected_identity'][sl['case']],'PCM/control/FP identity changed')
  need(row['cpu_start']==row['cpu_end']==receipt['pinned_cpu'],'affinity mismatch')
 regenerated=analyze(rows);published={k:v for k,v in expected.items() if k not in ('end_utc','plan_sha256','raw_sha256')}
 need(regenerated==published,'reported estimation, threshold or decision disagrees with raw observations')
 return regenerated
def read(p):
 fp=p/'raw.jsonl';return [json.loads(x) for x in fp.read_text().splitlines()],load(p/'result.json'),load(p/'receipt.json'),hashlib.sha256(fp.read_bytes()).hexdigest()
if __name__=='__main__':
 p=Path(__file__).resolve().parent;result=verify(*read(p));print(json.dumps({'slots':result['slots'],'primary':result['primary'],'gain':result['cases'][PRIMARY]['gain'],'ci95':result['cases'][PRIMARY]['ci95'],'primary_floor':result['primary_floor'],'admit_separate_native_study':result['candidate_for_separate_native_plugin_study']},indent=2))
