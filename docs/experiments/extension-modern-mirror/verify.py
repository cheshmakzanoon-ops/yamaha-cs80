#!/usr/bin/env python3
"""Read-only receipt/patch verifier. Never compiles, runs DSP or loads a plugin."""
import argparse
import hashlib
import json
from pathlib import Path
import re

COMMIT = 'fe13c25f354203838697b35adf3962bbe46735a4'
CHANGED = {'src/core/Engine.h', 'src/core/ModernLayer.h', 'src/core/ModernLayerMirror.cpp', 'src/plugin/ModernLayerState.cpp'}
ARCHIVE = '323db1f6f973508b9192f310f4bef66efd45e6c6178afc164dd5819b72b5e314'


def require(value, why):
    if not value:
        raise ValueError(why)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    def pairs(items):
        result = {}
        for k, v in items:
            require(k not in result, 'duplicate JSON key')
            result[k] = v
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=pairs)


def semantic(results, provenance, inventory):
    m, g, t = results['mirror'], results['guards'], results['traps']
    require(inventory['stock_commit'] == provenance['stock_commit'] == COMMIT, 'wrong baseline')
    require(inventory['stock_source_count'] == len(inventory['stock_sha256']) == 193, 'incomplete source inventory')
    require(set(inventory['candidate_overrides_sha256']) == set(provenance['changed_source_paths']) == CHANGED, 'wrong candidate source scope')
    require(all(k.startswith('src/') and '..' not in k for k in inventory['stock_sha256']), 'unsafe source path')
    require(all(re.fullmatch('[0-9a-f]{64}', v) for v in [*inventory['stock_sha256'].values(), *inventory['candidate_overrides_sha256'].values()]), 'bad source hash')
    require(provenance['stock_core_archive_sha256'] == ARCHIVE, 'wrong stock archive')
    require(provenance['prototype'] == 'frozen V1, research only', 'unqualified prototype')
    require(m['seeds'] == 64 and m['fp_contexts'] == 8 and m['attempts'] == 512, 'wrong corpus')
    require(m['accepted'] == 96 and m['transactional_fallbacks'] == 416 and m['accepted'] + m['transactional_fallbacks'] == m['attempts'], 'wrong admission counts')
    require(m['checks'] == 27873 == 1 + m['attempts'] * (4 + 24 * 2) + m['transactional_fallbacks'] * 3, 'wrong check count')
    require(m['continuation_steps_per_attempt'] == 24 and m['continuation_steps'] == 12288, 'wrong continuation coverage')
    require([m[k] for k in ('modern_config_bytes','modern_layer_bytes','engine_bytes')] == [1480,3992,290120], 'wrong ABI receipt')
    require(m['full_engine_render_or_audio_observed'] is False, 'direct scope inflated to audio')
    require(g['strict'] == {'cells':48,'accepted':2,'rejected':46}, 'wrong strict guard result')
    require(set(g['negative_new_TU_flags']) == {'-ffast-math','-mfma','-ffinite-math-only'}, 'wrong negative build scope')
    require(all(v == {'cells':48,'accepted':0,'rejected':48} for v in g['negative_new_TU_flags'].values()), 'negative compiler guard accepted')
    require(t['cases'] == 2 and t['children_per_case'] == 3 and t['require_actual_SIGFPE'] is True, 'trap coverage inflated or missing')
    require([(v['seed'],v['signal_code'],v['mxcsr_hex']) for v in t['observed']] == [(0,7,'1f01'),(1,5,'1e82')], 'wrong actual trap record')
    expected = {'mirror':['PASS checks=27873 accepted=96 rejected=416 ModernConfig=1480 ModernLayer=3992 Engine=290120'],
                'guard-strict':['PASS controls48 accepted=2 rejected=46 +pending x87/SSE'],
                **{k:['PASS controls48 accepted=0 rejected=48 +pending x87/SSE'] for k in ('guard-fast','guard-fma','guard-finite')},
                'traps':['PASS actual SIGFPE seed=0 code=7 csr=1f01 all3992statebytes + original PC + FP + errno; A/A passed',
                         'PASS actual SIGFPE seed=1 code=5 csr=1e82 all3992statebytes + original PC + FP + errno; A/A passed']}
    require(results['saved_stdout'] == expected, 'saved stdout differs from projected counts')
    v2 = results['rejected_revision']
    require(v2['runtime_executed'] is False and v2['stock_setter_bytes'] == v2['v2_setter_bytes'] == 4034 and v2['first_different_byte_offset_hex'] == '214', 'V2 rejection changed')
    require(provenance['direct_TU_flags_recorded'] == ['-O3','-DNDEBUG','-std=gnu++20','-fPIC'], 'direct flags changed')
    require(provenance['Ig00156Batch_extra_flags'] == ['-ffp-contract=off'], 'stock per-TU contraction contract missing')
    require(provenance['historical_direct_link_contract']['explicit_no_pie'] is False, 'unobserved link flag')
    require(all(provenance['export_verification'][k] == 0 for k in ('new_Cpp_builds','new_DSP_executions','timing_runs','native_loads')), 'export invents new runtime evidence')


def patch_sections(text):
    parts = re.split(r'^diff --git ', text, flags=re.M)
    require(parts[0] == '', 'unrecognized patch prefix')
    result = {}
    for part in parts[1:]:
        header, rest = part.split('\n', 1)
        a, b = header.split()
        require(a.startswith('a/src/') and b == 'b/' + a[2:] and '..' not in a, 'unsafe patch path')
        name = a[2:]
        require(name not in result, 'duplicate patch path')
        result[name] = rest.splitlines(keepends=True)
    require(set(result) == CHANGED, 'patch source scope differs')
    return result


def apply_section(old, section):
    out, cursor, i = [], 0, 0
    while i < len(section):
        if not section[i].startswith('@@ '):
            i += 1
            continue
        match = re.match(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@', section[i])
        require(match is not None, 'malformed patch hunk')
        start, count, newstart, newcount = (int(x) if x is not None else 1 for x in match.groups())
        target = start - 1 if count else start
        require(cursor <= target <= len(old), 'out-of-order patch hunk')
        out.extend(old[cursor:target]); cursor = target
        require(len(out) == newstart - 1, 'new hunk offset differs')
        used = made = 0; i += 1
        while i < len(section) and not section[i].startswith('@@ '):
            line = section[i]; prefix, value = line[:1], line[1:]
            require(prefix in (' ', '+', '-'), 'unrecognized patch line')
            if prefix in (' ', '-'):
                require(cursor < len(old) and old[cursor] == value, 'patch context differs')
                cursor += 1; used += 1
            if prefix in (' ', '+'):
                out.append(value); made += 1
            i += 1
        require((used, made) == (count, newcount), 'patch hunk counts differ')
    out.extend(old[cursor:])
    return ''.join(out)


def verify_source(root, inventory, patch):
    root = Path(root)
    for name, digest in inventory['stock_sha256'].items():
        require(sha((root/name).read_bytes()) == digest, 'stock source differs: ' + name)
    produced = {}
    for name, section in patch_sections(patch).items():
        old = (root/name).read_text().splitlines(keepends=True) if name in inventory['stock_sha256'] else []
        value = apply_section(old, section)
        require(sha(value.encode()) == inventory['candidate_overrides_sha256'][name], 'patch result differs: ' + name)
        produced[name] = value
    original = (root/'src/core/ModernLayer.h').read_text()
    candidate = produced['src/core/ModernLayer.h']
    def setter(text):
        return text.split('    void setConfig(const ModernConfig& newConfig) noexcept',1)[1].split('    const ModernConfig& getConfig()',1)[0]
    require(setter(original) == setter(candidate), 'original setter body changed')
    body = setter(original)
    tail = body[body.index('        for (int m = 0; m < kMsegCount; ++m)'):body.index('        rebuild();')+len('        rebuild();\n')]
    method = produced['src/core/ModernLayerMirror.cpp']
    duplicated = method[method.index('        for (int m = 0; m < kMsegCount; ++m)'):method.index('    return true;')]
    require(tail == duplicated.replace('modern.rebuild();','rebuild();'), 'duplicated continuation drift')
    return {'source_files':193,'patch_paths':4,'original_setter_source_unchanged':True,'continuation_source_equal':True}


def verify_package(directory, stock=None, manifest=True):
    directory = Path(directory)
    results, provenance, inventory = [read_json(directory/n) for n in ('direct-results.json','provenance.json','source-inventory.json')]
    semantic(results, provenance, inventory)
    patch = (directory/'candidate-v1.patch').read_text()
    patch_sections(patch)
    require(sha(patch.encode()) == provenance['candidate_patch_sha256'], 'patch hash mismatch')
    for name, value in provenance['bundled_test_sources_sha256'].items():
        require(sha((directory/name).read_bytes()) == value, 'test source changed')
    for p in directory.iterdir():
        if not p.is_file() or p.name == 'manifest.json':
            continue
        data = p.read_bytes()
        require(b'/' + b'workspace' + b'/' not in data and b'/' + b'home' + b'/' not in data and b'cpu-paired' + b'-clock-qualification/' not in data and b'cpu-extension-modern' + b'/candidate/' not in data, 'private path in export: ' + p.name)
    if manifest:
        m = read_json(directory/'manifest.json')
        actual = {p.name:sha(p.read_bytes()) for p in directory.iterdir() if p.is_file() and p.name != 'manifest.json'}
        require(m['files_sha256'] == actual, 'manifest content/inventory mismatch')
        require(m['scope'] == 'direct correctness only' and m['production_admission'] is False and m['performance_admission'] is False, 'manifest overclaims scope')
    details = verify_source(stock, inventory, patch) if stock is not None else None
    return {'status':'PASS offline receipts and hashes only','stock_patch_check':details,'new_runtime_evidence':False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stock', type=Path)
    args = p.parse_args()
    print(json.dumps(verify_package(Path(__file__).resolve().parent,args.stock),indent=2))


if __name__ == '__main__':
    main()
