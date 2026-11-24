#!/usr/bin/env python3
"""Write A46's final review from completed actual-evidence records only."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, required=True)
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--review', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    a = p.parse_args()
    r = json.loads((a.evidence / 'supporting-ci.json').read_text())
    e = json.loads((a.evidence / 'release-source-equivalence.json').read_text())
    assert r['status'] == 'completed' and r['conclusion'] == 'success'
    assert r['supporting_scope_qualified'] is True and r['native_release_qualified'] is False
    assert len(r['jobs']) == 5 and all(j['conclusion'] == 'success' for j in r['jobs'])
    assert len(r['actual_artifact_inspections']) == 6
    assert e['whole_supporting_test_input_tree_identical'] is False
    san = next(j for j in r['jobs'] if j['name'] == 'sanitizers')
    c = san['ctest']
    assert c['failed'] == 0 and not san['sanitizer_diagnostic_lines']
    prefix = '../ci/source-09af535/supporting-dsp/'
    lines = [
        '# A46 — actual supporting DSP qualification', '',
        '**All five original supporting jobs passed, and all six actual artifacts passed inspection.**', '',
        f"The executed source is `{r['source']}`, tree `{r['source_tree']}`.",
        f"The original run is [{r['run']['id']}, attempt {r['run']['run_attempt']}](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{r['run']['id']}).",
        'This review does not relabel those executions as the later native build source, qualify native installation bundles or claim acoustic equivalence.', '',
        '## Original job outcomes', '',
        '| Job | Job ID | Outcome |', '|---|---:|---|',
    ]
    for j in r['jobs']:
        lines.append(f"| {j['name']} | {j['id']} | {j['conclusion']} |")
    lines += ['', 'The exact-source checkout, source/run/attempt fields, complete original command logs and required successful steps are retained. Artifact uploads are bound by exact IDs, byte counts, SHA-256 digests and original timestamped upload records. Numeric artifact ordering was not used as provenance.', '',
              '## Actual supporting archives', '', '| Kind | Artifact ID | ZIP bytes | SHA-256 |', '|---|---:|---:|---|']
    for artifact in r['actual_artifact_inspections']:
        lines.append(f"| {artifact['kind']} | {artifact['artifact_id']} | {artifact['bytes']:,} | `{artifact['sha256']}` |")
    kinds = {x['kind']: x for x in r['actual_artifact_inspections']}
    lines += ['',
        'Every ZIP was hashed and inspected from the same immutable input bytes. All entries were checked for unique canonical paths, supported types and bounded sizes; complete file inventories and individual digests are retained. No downloaded code was executed.', '',
        f"The synthetic comparison has {kinds['synthetic']['checks']['trace_hashes_matched']} matching trace digests and {kinds['synthetic']['checks']['source_hashes_matched']} matching source digests; its manifest digest agrees and its explicit scope is synthetic, with `analog_evidence: false`.", '',
        f"The engine candidate and live-envelope exports have {kinds['candidate']['checks']['manifest_checksums_matched']} and {kinds['live-adr']['checks']['manifest_checksums_matched']} member checksums respectively. Each source manifest matches {kinds['candidate']['checks']['source_hashes_matched']} exact committed files. Both name the same producer executable digest; that binary is not distributed in these trace archives.", '',
        f"The numerical report records zero probe failures, a passed software-reference regression, a clean analyzed source tree and {kinds['dsp']['checks']['source_hashes_matched']} matching analysis-time source hashes. All parseable numeric cells across the downloaded CSVs are finite. This is not an independent recomputation of every numerical/Fourier result.", '',
        'The compatibility archives retain reports and matching comparison digests, plus the legacy corpus. Their original successful, unsuppressed `cmp` steps establish the raw byte comparisons. Raw comparison binaries are not uploaded, so this review does not claim a local raw-audio re-comparison. The fixed historical corpora are not a listening evaluation of the revised factory presets.', '',
        '## Sanitizer execution', '',
        f"The complete original terminal log records **{c['passed']} passed, {c['failed']} failed and {c['formal_skipped']} formally skipped CTest executions out of {c['registered']} registered entries**. All fifteen newly registered refinement tests are present and passed. These are core-build executions, including Python/tool fixtures as well as instrumented C++ tests; they remain separate from native host and pluginval totals.", '',
        'Formal skips: ' + (', '.join('`' + x['name'] + '`' for x in c['cases'] if x['result'] == '***Skipped') or 'none') + '.', '',
        'The actual workflow enabled ASan and UBSan, leak detection and halt-on-error. No matching AddressSanitizer, LeakSanitizer or UBSan diagnostic was found in the retained complete log. **Nested Python skips remain unknown** because successful child stdout is suppressed by `ctest --output-on-failure`; they are not inferred as zero.', '',
        'These supporting jobs build core-only targets or run Python fixtures. They do not build JUCE plugins and do not supply native JUCE provenance. The native workflow owns that separate requirement.', '',
        '## Relationship to the final native source', '',
        f"The later native build source is `{e['native_release_source']}`. The complete Git-tree comparison found exactly two changed files: `docs/INSTALL_AND_QUICKSTART.md` and `docs/PRESET_GUIDE.md`. All {e['identical_tracked_entries']:,} other tracked entries are identical, including {e['audio_circuit_fixture_workflow_code_and_data_entries_identical']} selected source, test, script, build, installer and asset entries.", '',
        'This establishes unchanged audio/circuit/fixture/workflow implementation and data, while preserving the original execution SHA. **The whole supporting test input tree is not identical:** `cs80_packaging_notice_tests` consumes both guides, and `cs80_macos_installer_architecture_tests` consumes the installation guide. The final-source native runs must qualify those document-dependent fixtures and the corrected packaged guides. This review does not assert independently built binaries are byte-identical.', '',
        '## Retained evidence and limits', '',
        f"[Actual supporting record]({prefix}supporting-ci.json) · [Source-equivalence record]({prefix}release-source-equivalence.json) · [Retained-file hashes]({prefix}retained-files.json)", '',
        'The authenticated API snapshots, complete decoded job logs, exact-source contracts, actual archive entry inventories and reproducible inspection scripts are retained alongside those records. The partial legacy-only observation remains explicitly partial. All observed results belong to the original supporting run and attempt.', '',
        'No production source, workflow, shared build, commit, remote ref or CI rerun was changed by A46. Numerical references, synthetic fixtures and compatibility checkpoints cannot establish an indistinguishable acoustic instrument, a physical CS-80 match, or a perceptual quality score.', '',
    ]
    with a.review.open('x', encoding='utf-8') as stream:
        stream.write('\n'.join(lines))
    paths = sorted(x for x in a.evidence.rglob('*') if x.is_file() and x != a.manifest)
    paths.append(a.review)
    files = []
    for path in paths:
        raw = path.read_bytes()
        files.append({'path': path.resolve().relative_to(a.repo.resolve()).as_posix(), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()})
    with a.manifest.open('x', encoding='utf-8') as stream:
        json.dump({'schema': 1, 'scope': 'A46 retained review/evidence files, excluding this self-referential manifest and scratch ZIP downloads.', 'files': files}, stream, indent=2); stream.write('\n')
    print(json.dumps({'review': str(a.review), 'retained_files': len(files), 'native_release_qualified': False}))


if __name__ == '__main__':
    main()
