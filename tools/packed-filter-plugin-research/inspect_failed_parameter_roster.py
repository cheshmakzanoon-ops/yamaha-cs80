"""Reconstruct all rejected-run metadata read-only; writes JSON only to stdout."""
import argparse,gzip,hashlib,io,json,pathlib
from failure_metadata_decode import decode_failure,classify_roster,MAX_BODY,require
ROOT=pathlib.Path(__file__).parent

def inspect(root):
    root=pathlib.Path(root);mp=root/'manifest.json';require(not mp.is_symlink() and mp.is_file() and mp.stat().st_size<=65536,'bounded evidence manifest')
    with mp.open('rb') as stream:manifest_bytes=stream.read(65537)
    require(len(manifest_bytes)<=65536,'evidence manifest grew');manifest=json.loads(manifest_bytes)
    require(manifest['schema']=='cs80-rejected-parameter-roster-evidence-v1' and manifest['admitted'] is False,'rejected evidence manifest')
    blobs={}
    for name in ('receipt.bin.gz','execution.json','expected.json','source-roster.json','report.json'):
        p=root/name;require(not p.is_symlink() and p.is_file() and p.stat().st_size<=MAX_BODY,'bounded evidence file');
        with p.open('rb') as stream:data=stream.read(MAX_BODY+1)
        require(len(data)<=MAX_BODY,'evidence file grew');row=manifest['files'][name];require(len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],'evidence file identity: '+name);blobs[name]=data
    with gzip.GzipFile(fileobj=io.BytesIO(blobs['receipt.bin.gz'])) as stream:raw=stream.read(MAX_BODY+81)
    require(len(raw)<=MAX_BODY+80 and len(raw)==manifest['uncompressed_receipt_bytes'] and hashlib.sha256(raw).hexdigest()==manifest['uncompressed_receipt_sha256'],'bounded decompressed receipt identity')
    execution=json.loads(blobs['execution.json']);require(execution['export_redaction']['kind']=='path-redacted-portable-export' and execution['export_redaction']['original_execution_json_sha256']==manifest['raw_original_execution_sha256'],'explicit path-redacted execution export');expected=json.loads(blobs['expected.json']);expected['nonce']=execution['argv'][3]
    require(execution['watchdog_reason'] is None and execution['group_empty_after_wait'] is True and execution['files']['receipt']['sha256']==manifest['uncompressed_receipt_sha256'],'recorded execution outcome')
    decoded=decode_failure(raw,expected=expected,observed_exit=execution['returncode'],observed_pid=execution['pid']);require(decoded['connected']['metadata_sha256']==manifest['metadata_sha256'],'retained metadata identity')
    source=json.loads(blobs['source-roster.json']);classification=classify_roster(decoded['connected'],source['generated'],source['memory'])
    return {'runtime_observation':decoded,'source_pattern_classification':classification,'recorded_execution':execution,'execution_paths_redacted':True,'not_exact_raw_argv_environment_evidence':True,'not_independent_runtime_attestation':True,'admitted':False,'production_qualified':False}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--full',action='store_true');p.add_argument('--evidence',type=pathlib.Path,default=ROOT/'evidence'/'failed-parameter-roster');args=p.parse_args();result=inspect(args.evidence)
    if not args.full:
        result={'admitted':False,'production_qualified':False,'metadata_sha256':result['runtime_observation']['connected']['metadata_sha256'],'parameter_count':result['runtime_observation']['connected']['parameter_count'],'failure_operation':result['runtime_observation']['history_operation'],'primary_error':result['runtime_observation']['primary_error'],'role_counts':result['source_pattern_classification']['role_counts'],'missing_generated':result['source_pattern_classification']['missing_generated'],'missing_memory':result['source_pattern_classification']['missing_source_memory']}
    print(json.dumps(result,indent=2))
