"""Read-only validation of retained stock history; not process attestation."""
import argparse,gzip,hashlib,io,json,pathlib
from identity import require
from process_lifetime_transaction import _payload,PAYLOAD_CAP
ROOT=pathlib.Path(__file__).parent

def bounded(path,cap):
    require(not path.is_symlink() and path.is_file() and path.stat().st_size<=cap,'bounded regular evidence file')
    with path.open('rb') as stream:data=stream.read(cap+1)
    require(len(data)<=cap,'evidence growth bound');return data

def inspect(root):
    root=pathlib.Path(root);manifest=json.loads(bounded(root/'manifest.json',128*1024))
    require(manifest['schema']=='cs80-historical-stock-history-diagnostic-v1' and manifest['production_qualified'] is False and manifest['candidate_compared'] is False,'stock diagnostic scope')
    blobs={}
    for name,cap in (('payload.bin.gz',PAYLOAD_CAP),('execution.json',128*1024),('report.json',128*1024),('payload-contract.json',128*1024),('source-expectations.txt',128*1024)):
        data=bounded(root/name,cap);expected=manifest['files'][name];require(len(data)==expected['bytes'] and hashlib.sha256(data).hexdigest()==expected['sha256'],'evidence file hash/size: '+name);blobs[name]=data
    with gzip.GzipFile(fileobj=io.BytesIO(blobs['payload.bin.gz'])) as stream:payload=stream.read(PAYLOAD_CAP+1)
    require(len(payload)<=PAYLOAD_CAP and len(payload)==manifest['payload_bytes'] and hashlib.sha256(payload).hexdigest()==manifest['payload_sha256'],'bounded decompressed payload identity')
    execution=json.loads(blobs['execution.json']);report=json.loads(blobs['report.json']);contract=json.loads(blobs['payload-contract.json'])
    require(execution['export_redaction']['kind']=='path-redacted-portable-export' and execution['export_redaction']['original_execution_json_sha256']==manifest['original_execution_sha256'],'explicit execution sanitization')
    require(report['export_redaction']['kind']=='provider-paths-omitted-portable-export' and report['export_redaction']['original_report_sha256']==manifest['original_report_sha256'],'explicit report sanitization')
    require(execution['returncode']==0 and execution['watchdog_reason'] is None and execution['group_empty_after_wait'] is True and execution['files']['receipt']['sha256']==manifest['raw_receipt_sha256'],'recorded execution outcome')
    require(contract['arm']=='stock' and contract['parameter_binding_policy']==2 and contract['history_schema']=='cs80-instance-history-A-D-v2','frozen stock source-aware contract')
    pure=blobs['source-expectations.txt'].decode().splitlines();require(len(pure)==6 and pure[2]==contract['case_identity_hex'] and pure[3:]==contract['protocol_sha256'],'source-only expected case/protocol identity')
    for name,row in manifest['decoder_files'].items():
        data=bounded(ROOT/name,2*1024*1024);require(hashlib.sha256(data).hexdigest()==row,'frozen decoder/helper source changed: '+name)
    proof=_payload(payload,bytes.fromhex(contract['case_identity_hex']),contract['controller_state_policy'],contract['controller_state_result'],contract)
    for key,value in proof.items():require(report[key]==value,'retained report differs from decoded payload: '+key)
    require(report['receipt_sha256']==manifest['raw_receipt_sha256'] and report['payload_sha256']==manifest['payload_sha256'],'recorded receipt/payload hashes')
    return {'kind':'historical-stock-only-one-case-diagnostic','production_qualified':False,'candidate_compared':False,'CPU_timing_claim':False,'whole_grid_run':False,'independent_execution_attestation':False,'raw_receipt_retained':False,'provider_retention_is_recorded_not_reverified':True,'provider_disposition':report['provider_disposition'],'payload_sha256':manifest['payload_sha256'],'payload_bytes':len(payload),'source_history_proof':proof}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--evidence',type=pathlib.Path,default=ROOT/'evidence'/'stock-history-diagnostic');args=parser.parse_args();print(json.dumps(inspect(args.evidence),indent=2))
