#!/usr/bin/env python3
"""Derive source continuity from frozen Git objects, without accepting artifacts."""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

OLD = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
OLD_TREE = '469244af34233cfa145537266efd29024b66abba'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
MANIFEST = 'docs/preset-evidence/2026.11.7/development/source-inputs-c5549f7d.json'
INVENTORY = 'docs/preset-evidence/2026.11.7/verification/a12/inventory.json'
INVENTORY_SHA = '55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d'
ROOTS = ('.github', 'CMakeLists.txt', 'cmake', 'scripts', 'src', 'tests')
CHANGED = {'src/tools/parity_capture.cpp', 'tests/LibraryAuditTests.cpp'}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def record(raw):
    return dict(bytes=len(raw), sha256=sha(raw))


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def tree(repo, source):
    result = {}
    for entry in git(repo, 'ls-tree', '-r', '-z', '--full-tree', source).split(b'\0'):
        if not entry:
            continue
        metadata, path = entry.split(b'\t', 1)
        mode, kind, oid = metadata.decode().split()
        result[path.decode()] = dict(mode=mode, type=kind, oid=oid)
    return result


def blobs(repo, oids):
    ordered = sorted(set(oids))
    raw = subprocess.check_output(['git', '-C', str(repo), 'cat-file', '--batch'],
                                  input=('\n'.join(ordered) + '\n').encode())
    result, offset = {}, 0
    for expected in ordered:
        end = raw.index(b'\n', offset)
        oid, kind, length = raw[offset:end].decode().split()
        assert oid == expected and kind == 'blob'
        offset = end + 1
        size = int(length)
        result[oid] = raw[offset:offset + size]
        offset += size
        assert raw[offset:offset + 1] == b'\n'
        offset += 1
    assert offset == len(raw)
    return result


def write_fresh(path, value):
    with path.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def normalize_ownership(path, raw):
    text = raw.decode()
    assert text.count('#include <memory>\n') == 1
    text = text.replace('#include <memory>\n', '')
    if path == 'tests/LibraryAuditTests.cpp':
        comment = "// Keep each fresh engine off the helper thread's limited stack in this offline audit.\n"
        assert text.count(comment) == 1
        text = text.replace(comment, '')
        before = 'const auto engine = std::make_unique<Engine>();\n        auto& e = *engine;'
        assert text.count(before) == 2
        text = text.replace(before, 'Engine e;')
    else:
        for indent in ('    ', '        '):
            before = ('\n' + indent + 'auto engineStorage = std::make_unique<Engine>();\n'
                      + indent + 'auto& engine = *engineStorage;')
            assert text.count(before) == 1
            text = text.replace(before, '\n' + indent + 'Engine engine;')
    return text.encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    repo, out = args.repo.resolve(), args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for source, expected in ((OLD, OLD_TREE), (SOURCE, TREE)):
        assert git(repo, 'rev-parse', source + '^{tree}').decode().strip() == expected
    manifest_raw = (repo / MANIFEST).read_bytes()
    assert manifest_raw == git(repo, 'show', SOURCE + ':' + MANIFEST)
    manifest = json.loads(manifest_raw)
    assert manifest['source'] == OLD and manifest['tree'] == OLD_TREE
    old_tree, new_tree = tree(repo, OLD), tree(repo, SOURCE)
    protected = lambda rows: {p: d for p, d in rows.items() if p.split('/')[0] in ROOTS}
    old_inputs, new_inputs = protected(old_tree), protected(new_tree)
    assert len(old_inputs) == len(new_inputs) == manifest['file_count'] == 945
    assert old_inputs.keys() == new_inputs.keys() == manifest['inputs'].keys()
    content = blobs(repo, [d['oid'] for rows in (old_inputs, new_inputs) for d in rows.values()])
    old_records = {p: record(content[d['oid']]) for p, d in old_inputs.items()}
    new_records = {p: record(content[d['oid']]) for p, d in new_inputs.items()}
    assert old_records == manifest['inputs']
    changed = {p for p in old_inputs if old_inputs[p] != new_inputs[p]}
    assert changed == CHANGED
    assert all(old_inputs[p]['mode'] == new_inputs[p]['mode'] for p in old_inputs)
    normalization = {}
    for path in sorted(changed):
        before, after = content[old_inputs[path]['oid']], content[new_inputs[path]['oid']]
        normalized = normalize_ownership(path, after)
        assert normalized == before
        normalization[path] = dict(before=record(before), after=record(after),
                                   new_engine_owners=2, normalized_bytes_equal_old_source=True)
    all_changed = [dict(path=p, before=old_tree.get(p), after=new_tree.get(p))
                   for p in sorted(old_tree.keys() | new_tree.keys())
                   if old_tree.get(p) != new_tree.get(p)]
    remaining = [r for r in all_changed if r['path'] not in CHANGED]
    assert len(all_changed) == 373 and len(remaining) == 371
    assert all(r['path'].startswith(('docs/preset-evidence/', 'docs/screenshots/')) for r in remaining)
    identity_trees = {}
    for path in ('.github', 'cmake', 'scripts', 'src/core', 'src/experimental',
                 'src/plugin', 'src/reference', 'src/vendor', 'assets', 'installer'):
        before = git(repo, 'rev-parse', OLD + ':' + path).decode().strip()
        after = git(repo, 'rev-parse', SOURCE + ':' + path).decode().strip()
        assert before == after
        identity_trees[path] = dict(old_git_tree=before, new_git_tree=after, identical=True)
    extra_inputs = {}
    for path in ('docs/IG00156_NODAL_MODEL.md', 'docs/IG00151_VCA_MODEL.md',
                 'docs/VCA_REFERENCE.md', 'docs/INSTALL_AND_QUICKSTART.md',
                 'docs/PRESET_GUIDE.md', 'docs/CI_DISTRIBUTION_STATUS.md', 'LICENSE'):
        before, after = (git(repo, 'show', source + ':' + path) for source in (OLD, SOURCE))
        assert before == after
        extra_inputs[path] = record(after)
    inventories = [git(repo, 'show', source + ':' + INVENTORY) for source in (OLD, SOURCE)]
    assert inventories[0] == inventories[1] == (repo / INVENTORY).read_bytes()
    assert sha(inventories[0]) == INVENTORY_SHA
    bank = json.loads(inventories[0])
    ids, programs = bank['parameter_ids'], bank['programs']
    assert bank['library_revision'] == '2026.11.7'
    assert len(ids) == len(set(ids)) == 204 and len(programs) == 523
    assert len({p['name'] for p in programs}) == 523
    samples, identities = bytearray(), []
    for index, program in enumerate(programs):
        assert program['index'] == program['host_index'] == index
        values = program['values']
        assert len(values) == 204 and all(type(v) in (int, float) and math.isfinite(v) for v in values)
        samples.extend(struct.pack('<204f', *values))
        identities.append({k: v for k, v in program.items() if k != 'values'})
    new_manifest = dict(schema=1, source=SOURCE, tree=TREE,
                        purpose='Frozen protected input identities; no native/artifact qualification.',
                        file_count=945, inputs=new_records)
    write_fresh(out / 'source-inputs-ee0dabca.json', new_manifest)
    write_fresh(out / 'complete-tree-changed-files.json', dict(schema=1, old_source=OLD,
                source=SOURCE, changed_path_count=len(all_changed), files=all_changed))
    patch = git(repo, 'diff', '--no-ext-diff', OLD, SOURCE, '--', *sorted(CHANGED))
    with (out / 'protected-source-difference.patch').open('xb') as stream:
        stream.write(patch)
    important = ('CMakeLists.txt', '.github/workflows/build.yml', 'scripts/capture-factory-auditions.py',
                 'src/tools/preset_capture.cpp', 'src/core/FactoryPresets.h', 'src/core/FactoryPresets.cpp',
                 'src/core/Engine.h', 'src/core/Engine.cpp', 'src/core/InstrumentBody.h',
                 'src/tools/ui_snapshot.cpp', 'tests/InstrumentBodyHostTests.h')
    assert all(old_records[p] == new_records[p] for p in important)
    report = dict(schema=1, status='source_continuity_verified_not_binary_or_artifact_acceptance',
        factory_revision='2026.11.7', old_source=OLD, old_tree=OLD_TREE, source=SOURCE, tree=TREE,
        run_id=45397189410, attempt=1,
        producer_job_ids={'linux-x86_64':138455975798, 'windows-x86_64':138455976256,
                          'macos-arm64':138455976227, 'macos-x86_64':138455976307},
        retained_old_manifest=dict(path=MANIFEST, **record(manifest_raw),
                                   all_945_entries_independently_matched_to_old_git_blobs=True,
                                   bytes_match_new_frozen_manifest=True),
        protected_roots=list(ROOTS), protected_file_count=945, identical_protected_files=943,
        protected_path_sets_identical=True, protected_file_modes_identical=True,
        root_counts=dict(collections.Counter(p.split('/')[0] for p in old_inputs)),
        old_canonical_input_records_sha256=sha(canonical(old_records)),
        new_canonical_input_records_sha256=sha(canonical(new_records)),
        exact_ownership_only_changes=normalization,
        complete_tree_changed_paths=373, other_changes=371,
        other_changes_scope='Only docs/preset-evidence and docs/screenshots; see complete-tree-changed-files.json.',
        unchanged_git_trees=identity_trees, unchanged_critical_inputs={p:new_records[p] for p in important},
        unchanged_additional_documentation_and_license_inputs=extra_inputs,
        bank_identity=dict(path=INVENTORY, **record(inventories[0]), programs=523, parameters=204,
            values=106692, inventory_bytes_equal_in_both_frozen_sources=True,
            all_ordered_identities_and_metadata_identical=True, all_binary32_parameter_values_identical=True,
            ordered_parameter_ids_sha256=sha(canonical(ids)),
            ordered_program_metadata_sha256=sha(canonical(identities)),
            ordered_binary32_parameter_values_sha256=sha(bytes(samples)),
            runtime_inventory_from_new_artifact_checked=False),
        source_sound_continuity_basis='All product core, plugin, reference and vendor trees, factory data, parameter definitions, audition producer, curated capture executable source, build recipe and workflow bytes remain identical. The only protected edits change ownership of four fresh Engine objects in two offline harnesses. Removing exactly those ownership edits reconstructs each old file byte for byte; Engine construction and call order remain unchanged.',
        retained_audio_reviews_applicable_to_unchanged_sound_source=True,
        retained_audio_reviews_scope='Prior local/reference listening and measurement reviews remain evidence about the identical audio implementation and bank, with their original source/run and original limitations. They are not relabeled as measurements of ee0dabca native artifacts.',
        binary_byte_identity_claimed=False, new_pcm_byte_identity_claimed=False,
        original_c554_platform_or_artifact_receipts_rebound=False,
        actual_artifact_checked=False, native_results_accepted=False, accepted_archive_content=False,
        audition_qualified=False, platform_qualified=False, release_qualified=False,
        remaining_requirements='Fresh ee0dabca native, validator, package, original-process/upload, payload and audition checks; old failed gates remain failed and old accepted receipts retain their original source.',
        generated_records={name:record((out/name).read_bytes()) for name in (
            'source-inputs-ee0dabca.json', 'complete-tree-changed-files.json', 'protected-source-difference.patch')})
    write_fresh(out / 'source-continuity.json', report)
    print(json.dumps(dict(status=report['status'], identical_protected_files=943, changed_protected_files=2,
                          inventory_sha256=INVENTORY_SHA, release_qualified=False)))


if __name__ == '__main__':
    main()
