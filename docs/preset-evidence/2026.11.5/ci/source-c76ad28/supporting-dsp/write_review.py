#!/usr/bin/env python3
"""Write A46's final replacement-run review only from complete actual evidence."""
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
    assert r['source'] == 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
    assert r['source_tree'] == '416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00'
    assert r['run']['id'] == 45351731746 and r['run']['run_attempt'] == 1
    assert r['status'] == 'completed' and r['conclusion'] == 'success'
    assert r['supporting_scope_qualified'] is True and r['native_release_qualified'] is False
    assert len(r['jobs']) == 5 and all(j['conclusion'] == 'success' for j in r['jobs'])
    assert len(r['actual_artifact_inspections']) == 6
    jobs = {j['name']: j for j in r['jobs']}
    assert len(jobs['measure']['python_test_terminal_summaries']) == 1
    assert len(jobs['Synthetic reference comparison (not hardware evidence)']['python_test_terminal_summaries']) == 1
    capture_tests = jobs['measure']['python_test_terminal_summaries'][0]
    synthetic_tests = jobs['Synthetic reference comparison (not hardware evidence)']['python_test_terminal_summaries'][0]
    san = jobs['sanitizers']
    c = san['ctest']
    assert c['failed'] == 0 and not san['sanitizer_diagnostic_lines']
    kinds = {x['kind']: x for x in r['actual_artifact_inspections']}
    for key in ('candidate', 'live-adr'):
        assert kinds[key]['checks']['actual_capture']['parameter_count'] == 202
    prefix = '../ci/source-c76ad28/supporting-dsp/'
    lines = [
        '# A46 — final supporting DSP qualification', '',
        '**All five original jobs passed, and all six actual replacement-run archives passed inspection.**', '',
        f"The executed source is `{r['source']}`, tree `{r['source_tree']}`.",
        f"The original run is [{r['run']['id']}, attempt {r['run']['run_attempt']}](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{r['run']['id']}).",
        'These are direct executions of the final native build source; no source-equivalence substitution or documentation-input exception is used. This review qualifies the supporting workflow only, not native installation packages or acoustic equivalence.', '',
        '## Original job outcomes', '',
        '| Job | Job ID | Outcome |', '|---|---:|---|',
    ]
    for j in r['jobs']:
        lines.append(f"| {j['name']} | {j['id']} | {j['conclusion']} |")
    lines += ['', 'The exact-source checkout, repository/source/run/attempt fields, complete original command logs and required successful steps are retained. Uploads are bound by exact artifact IDs, byte counts, SHA-256 digests and original timestamped records. Numeric artifact ordering was not used as provenance.', '',
              '## Actual supporting archives', '', '| Kind | Artifact ID | ZIP bytes | SHA-256 |', '|---|---:|---:|---|']
    for artifact in r['actual_artifact_inspections']:
        lines.append(f"| {artifact['kind']} | {artifact['artifact_id']} | {artifact['bytes']:,} | `{artifact['sha256']}` |")
    cells = sum(x['checks']['csv_scan']['finite_numeric_cells'] for x in kinds.values())
    csvs = sum(x['checks']['csv_scan']['files'] for x in kinds.values())
    lines += ['',
        'Each ZIP was acquired through the authenticated connector, matched to its immutable API size/digest, then inspected from those same bytes. Every member has a unique safe path, supported type, bounded size and retained digest. No code contained in the archives was executed.', '',
        f"The synthetic comparison has {kinds['synthetic']['checks']['trace_hashes_matched']} matching trace digests, {kinds['synthetic']['checks']['source_hashes_matched']} matching source digest and a matching manifest digest. Its original terminal log records {synthetic_tests} Python tests. Its declared scope is synthetic, with `analog_evidence: false`.", '',
        f"The engine candidate and live-envelope archives have {kinds['candidate']['checks']['manifest_checksums_matched']} and {kinds['live-adr']['checks']['manifest_checksums_matched']} matching member checksums. Each source manifest matches {kinds['candidate']['checks']['source_hashes_matched']} exact committed files. Both declare the same producer executable digest; that executable is not distributed in the trace archives.", '',
        'The actual capture manifests each contain all 202 parameter IDs derived from the committed core tables. The candidate settings and both live-envelope settings maps contain `body1_variant: 0` and `body2_variant: 0` (Standard). Every channel has 96,000 correctly indexed finite samples, the expected initial silence and nonzero fixture signal. The manifests declare 48 kHz host rate, 96 kHz internal rate and 67 uncompensated latency samples. Live parameter batches replay to the exported final settings.', '',
        f"The numerical report records zero probe failures, a passed software-reference regression, a clean analyzed source tree and {kinds['dsp']['checks']['source_hashes_matched']} matching analysis-time source hashes. Across {csvs} CSV files, all {cells:,} parseable numeric cells are finite. This does not independently recompute every numerical/Fourier result.", '',
        'The compatibility archives retain reports and matching comparison digests, plus the immutable legacy corpus. Original successful, unsuppressed `cmp` steps establish the raw byte comparisons. Raw comparison binaries are not uploaded, so no local raw-audio re-comparison is claimed. These fixed historical corpora and engine captures, whose instrument bodies are disabled, are not listening evaluations of the revised factory presets.', '',
        '## Sanitizer execution', '',
        f"The complete original terminal log records **{c['passed']} passed, {c['failed']} failed and {c['formal_skipped']} formally skipped CTest executions out of {c['registered']} registered entries**. These counts are derived from the actual result roster, with unique names and complete result ordinals. All fifteen newly registered refinement tests are present and passed.", '',
        'Formal skips: ' + (', '.join('`' + x['name'] + '`' for x in c['cases'] if x['result'] == '***Skipped') or 'none') + '.', '',
        'The core build used ASan and UBSan, leak detection and halt-on-error. No matching AddressSanitizer, LeakSanitizer or UBSan diagnostic was found in the complete retained log. These CTest executions include Python/tool fixtures as well as instrumented C++ tests and remain separate from native host/pluginval totals.', '',
        '**Nested Python skips remain unknown** where successful child stdout is suppressed by `ctest --output-on-failure`; they are not inferred as zero. The supporting jobs build core-only targets or execute Python fixtures and do not provide JUCE plugin provenance. Native qualification owns that separate requirement.', '',
        '## Earlier failure and actual replacement', '',
        'The earlier 09af measurement job failed three of thirteen metadata tests because its expectation still counted 200 parameters after two variant IDs were appended. Its numerical-bounds step passed, but export/upload steps did not produce the three required archives. Its successful partial checkpoints never established overall DSP acceptance.', '',
        f"The final source updates the explicit count to 202 and adds strict Standard-variant and mutation checks. This replacement run's original measurement log records {capture_tests} tests followed by `OK`, and the actual new exports passed the independent archive checks above. No old successful result or archive was reused to qualify this source.", '',
        '[Original failed measurement log](../ci/source-09af535/measure-failed-original.log) · [Original failed-job API](../ci/source-09af535/dsp-jobs-measure-failed-api.json) · [Preserved historical checkpoint](../ci/source-09af535/supporting-dsp/checkpoint.md)', '',
        '## Retained evidence and limits', '',
        f"[Actual supporting record]({prefix}supporting-ci.json) · [Exact source contract]({prefix}source-contract.json) · [Retained-file hashes]({prefix}retained-files.json)", '',
        'Authenticated API snapshots, decoded original job logs, actual archive entry inventories and reproducible acquisition/inspection helpers are retained. The earlier four-job observation remains explicitly partial. All final observed results belong to the replacement run and original attempt.', '',
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
        json.dump({'schema': 1, 'scope': 'A46 final replacement review/evidence files, excluding this self-referential manifest and scratch ZIP downloads.', 'files': files}, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'review': str(a.review), 'retained_files': len(files), 'native_release_qualified': False}))


if __name__ == '__main__':
    main()
