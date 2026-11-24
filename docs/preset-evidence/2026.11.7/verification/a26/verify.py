#!/usr/bin/env python3
"""A26 Hardware Studies review; only documentation evidence and raw scratch output.

All 22 programs receive structural and current soft/firm capture coverage.
Original-source checks reuse .6 A30's unmodified probe and the existing source
nominal suite. Six stack cases reuse LibraryAuditTests.cpp's measure/limits.
No reference-library custom probe is compiled, so no cross-version ABI is used.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
EVIDENCE = ROOT / 'docs/preset-evidence'
FROZEN = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
SOURCE6 = '18711f93a39aa49f3d6f9a41784be456224facbe'
SOURCE4 = '8685e9ed39d6347b67401da7590b7e1e8e95472d'
MODELS = ['Off', 'Piano strings', 'Tine electric piano', 'Reed electric piano',
          'Clavichord', 'Harpsichord', 'Nylon strings', 'Steel strings',
          'Plucked bass', 'Bowed strings', 'Flute', 'Clarinet', 'Oboe',
          'Bassoon', 'Brass', 'Organ', 'Bell', 'Vibraphone', 'Marimba', 'Celesta']
TEMPLATES = [
    'Ensemble Strings', 'Chamber Strings', 'Slow Strings', 'PWM Strings',
    'Brass Swell I', 'Brass Swell II', 'Flute', 'Horn Section',
    'Electric Piano I', 'Analog Bass', 'Clavichord I', 'Clavichord II',
    'Harpsichord I', 'Harpsichord II', 'Flute Organ 8+4', 'Reed Organ',
    'Guitar I', 'Guitar II', 'Funky Pluck I', 'Funky Pluck II',
    'Funky Clavichord', 'Funky Pluck I']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git_bytes(commit, path):
    return subprocess.check_output(['git', 'show', f'{commit}:{path}'], cwd=ROOT)


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=Path('/tmp/cs80-build-1cee5a'))
    parser.add_argument('--raw', type=Path, default=Path('/tmp/cs80-a26-1cee5a'))
    args = parser.parse_args()
    args.raw.mkdir(parents=True, exist_ok=True)
    commands = []

    def run(command, log=None):
        command = [str(x) for x in command]
        commands.append(command)
        if log:
            with (OUT / log).open('w') as stream:
                subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
        else:
            subprocess.run(command, cwd=ROOT, check=True)

    sources = {}
    for rel in ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h',
                'src/core/FactoryPresets.cpp', 'src/core/FactoryPresets.h',
                'src/core/OriginalCircuitLibrary.inc', 'src/core/InstrumentBody.h',
                'src/core/SourceFactoryTones.h', 'src/core/HardwareControl.h',
                'src/core/FactoryToneHypotheses.h', 'src/core/MemoryPanels.h',
                'src/plugin/PluginProcessor.cpp', 'src/plugin/PluginProcessor.h',
                'src/plugin/CentreRow.cpp', 'tests/InstrumentBodyHostTests.h',
                'tests/LibraryAuditTests.cpp', 'tests/SourceFactoryToneTests.cpp',
                'tests/BodyArticulationEngineTests.cpp', 'tests/KeyboardResponseTests.cpp',
                'src/tools/preset_capture.cpp']:
        raw = (ROOT / rel).read_bytes()
        assert raw == git_bytes(FROZEN, rel), rel
        sources[rel] = sha(raw)
    inventory_path = EVIDENCE / '2026.11.7/verification/a12/inventory.json'
    current_raw = inventory_path.read_bytes()
    current = json.loads(current_raw)
    base_raw = git_bytes(SOURCE6, 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json')
    assert base_raw.replace(b'"library_revision":"2026.11.6"', b'"library_revision":"2026.11.7"', 1) == current_raw
    base = json.loads(base_raw)
    original = json.loads(git_bytes(SOURCE4, 'docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json'))
    original_by_id = {p['host_index']: p for p in original['programs']}
    ids = current['parameter_ids']
    assert len(ids) == 204
    programs = [p for p in current['programs'] if p['category'] == 'Hardware Studies']
    assert len(programs) == 22 and [p['host_index'] for p in programs] == list(range(22))
    assert len({tuple(v for i, v in enumerate(p['values']) if ids[i] != 'volume') for p in programs}) == 22
    coverage = json.loads((EVIDENCE / '2026.11.7/verification/a12/mechanism-coverage.json').read_text())
    mechanism_by_id = {p['host_index']: p['eligible_mechanisms'] for p in coverage['programs']}
    previous_a30 = EVIDENCE / '2026.11.6/verification/a30'
    previous_rows = json.loads((previous_a30 / 'structural-audit.json').read_text())['programs']
    rows = []
    for p in programs:
        host = p['host_index']
        assert p == base['programs'][host]
        old = original_by_id[host]
        assert all(p[key] == old[key] for key in ['index', 'host_index', 'name', 'category'])
        values = dict(zip(ids, p['values']))
        assert values['parity_mode'] == 0 and values['source_performance'] == 1
        assert (values['mix'] < .5) == (host % 2 == 0)
        changes = [ids[i] for i in range(200) if ids[i] != 'volume' and p['values'][i] != old['values'][i]]
        assert changes == previous_rows[host]['changed_prior_non_volume_controls']
        active = []
        mechanisms = set()
        for rank in [1, 2]:
            model = int(values[f'body{rank}_model'])
            mix = values[f'body{rank}_mix']
            assert values[f'body{rank}_material'] == values[f'body{rank}_variant'] == 0
            if not (model and mix):
                continue
            assert 0 < values['mix'] < 1
            assert any(values[f'ch{rank}_{key}'] > 0 for key in ['v_level', 'init_level', 'after_level'])
            active.append({'rank': rank, 'model': model, 'name': MODELS[model], 'mix': mix})
            mechanisms.add('body')
            if model != 15:
                mechanisms.add('noise_rate_normalization')
            if model in [4, 5, 6, 7, 8]:
                mechanisms.add('generic_string_formants')
            if model in [9, 10, 11, 12, 13, 14]:
                mechanisms.add('sustained_air_and_flutter_subset')
            if model == 1:
                mechanisms.add('piano_partial_decay')
        assert sorted(mechanisms) == sorted(mechanism_by_id[host])
        rows.append({'host_index': host, 'name': p['name'], 'authored_template': TEMPLATES[host],
                     'named_rank': host % 2 + 1, 'rank_ii_weight': values['mix'],
                     'parameters_unchanged_vs_6': 204, 'active_bodies': active,
                     'neutral_material_and_variant': True,
                     'changed_non_volume_controls_vs_4': changes,
                     'eligible_mechanisms': sorted(mechanisms)})
    write('program-audit.json', {
        'program_count': 22, 'unchanged_values_vs_6': 22 * 204,
        'body_programs': sum(bool(p['active_bodies']) for p in rows),
        'active_body_ranks': sum(len(p['active_bodies']) for p in rows),
        'new_7_parameter_edits': 0,
        'mechanism_program_counts': {key: sum(key in p['eligible_mechanisms'] for p in rows)
            for key in ['piano_partial_decay', 'noise_rate_normalization', 'generic_string_formants',
                        'low_choir_harmonics', 'sustained_air_and_flutter_subset']},
        'programs': rows})

    capture = args.build / 'cs80_preset_capture'
    nominal = args.build / 'cs80_source_factory_tone_tests'
    core = args.build / 'libcs80_core.a'
    executables = {str(p): sha(p.read_bytes()) for p in [capture, nominal, core]}
    routing_source = previous_a30 / 'material-routing.cpp'
    assert routing_source.read_bytes() == git_bytes(SOURCE6, str(routing_source.relative_to(ROOT)))
    for source, name in [(routing_source, 'material-routing'), (OUT / 'review_probe.cpp', 'review-probe')]:
        run(['c++', '-std=c++20', '-O2', '-DCS80_ENABLE_BORROWED_HELD=1', '-I', ROOT / 'src',
             '-I', ROOT, source, core, '-pthread', '-o', args.raw / name], f'{name}-compile.log')
    run([args.raw / 'material-routing'], 'material-routing.log')
    assert '442 checks, 0 failures' in (OUT / 'material-routing.log').read_text()
    run([nominal], 'source-factory-tones.log')
    assert '378861 source factory nominal-model checks; 0 failures' in (OUT / 'source-factory-tones.log').read_text()
    run([args.raw / 'review-probe', args.raw], 'review-probe.log')
    for name in ['source-columns.csv', 'representative-stacks.csv']:
        shutil.copyfile(args.raw / name, OUT / name)
    columns = list(csv.DictReader((OUT / 'source-columns.csv').open()))
    assert len(columns) == 22 and sum(p['route'] == 'source_complete' for p in columns) == 7
    assert all(p['unknown_mask'] == p['inferred_mask'] and p['hardware_measured'] == '0' for p in columns)
    stacks = list(csv.DictReader((OUT / 'representative-stacks.csv').open()))
    assert len(stacks) == 6 and all(p['existing_limits_pass'] == '1' for p in stacks)

    for label, velocity in [('soft', '.25'), ('firm', '.85')]:
        command = [capture, '--out', args.raw / label, '--library', 'all', '--rate', '48000',
                   '--note', '60', '--velocity', velocity, '--pressure', '0', '--hold', '1.4', '--tail', '.5']
        for p in programs:
            command += ['--preset', p['name']]
        run(command, f'{label}-capture.log')
        shutil.copyfile(args.raw / label / 'metrics.csv', OUT / f'{label}-metrics.csv')
    metrics = {label: {p['name']: p for p in csv.DictReader((OUT / f'{label}-metrics.csv').open())}
               for label in ['soft', 'firm']}
    assert all(len(p) == 22 for p in metrics.values())
    touches = []
    for p in programs:
        soft, firm = [metrics[label][p['name']] for label in ['soft', 'firm']]
        for metric in [soft, firm]:
            assert metric['nonfinite'] == metric['samples_over_full_scale'] == '0'
            assert math.isfinite(float(metric['peak_dbfs'])) and float(metric['peak_dbfs']) < 0
        touches.append({'name': p['name'],
            'early_soft_firm_contrast_db': float(firm['early_rms_dbfs']) - float(soft['early_rms_dbfs']),
            'firm_peak_dbfs': float(firm['peak_dbfs']), 'firm_body_rms_dbfs': float(firm['body_rms_dbfs']),
            'firm_tail_rms_dbfs': float(firm['tail_rms_dbfs'])})
    effort = [p['early_soft_firm_contrast_db'] for p in touches if 'Organ' not in p['name'] and 'Harpsichord' not in p['name']]
    write('capture-summary.json', {'capture_count': 44, 'program_count': 22, 'rate': 48000,
        'note': 60, 'velocities': [.25, .85], 'pressure': 0, 'hold_seconds': 1.4,
        'tail_seconds': .5, 'all_finite_unclipped': True, 'effort_led_count': len(effort),
        'effort_led_early_contrast_db_range': [min(effort), max(effort)],
        'maximum_peak_dbfs': max(float(p['peak_dbfs']) for group in metrics.values() for p in group.values()),
        'scope': 'Basic complete-program touch/output observations; no listening or acoustic equivalence verdict.',
        'programs': touches})

    focused = EVIDENCE / '2026.11.7/development'
    receipt = json.loads((focused / 'focused-ctest-receipt.json').read_text())
    assert receipt['source'] == FROZEN and receipt['passed'] == 31 and receipt['failed'] == 0
    for filename, digest in receipt['files'].items():
        assert sha((focused / filename).read_bytes()) == digest
    tree = ET.parse(focused / 'cs80-focused.xml')
    cases = list(tree.getroot().iter('testcase'))
    assert len(cases) == 31 and not any(c.find('failure') is not None for c in cases)
    selected = ['cs80_library_audit_tests', 'cs80_body_articulation_engine_tests',
                'cs80_keyboard_response_tests', 'cs80_body_noise_rate_tests', 'cs80_string_body_formant_tests']
    selected_cases = []
    for case in cases:
        if case.attrib['name'] in selected:
            output = case.findtext('system-out') or ''
            selected_cases.append({'name': case.attrib['name'], 'status': 'passed',
                'seconds': float(case.attrib['time']), 'output_truncated_by_ctest': 'output was removed' in output,
                'actual_output': output})
    write('reused-focused-tests.json', {'source': FROZEN, 'freshly_rerun_by_a26': False,
        'focused_suite_passed': 31, 'selected_testcases': selected_cases,
        'library_audit_scope': 'All 523 complete saved programs, including all 22 Hardware Studies; not original hardware-tab electrical sounds.',
        'source_receipt_sha256': sha((focused / 'focused-ctest-receipt.json').read_bytes())})
    for path, digest in executables.items():
        assert sha(Path(path).read_bytes()) == digest
    write('receipt.json', {'review_source': FROZEN,
        'observed_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'baseline_6_source': SOURCE6, 'published_4_source': SOURCE4,
        'inventory_sha256': sha(current_raw), 'baseline_6_inventory_sha256': sha(base_raw),
        'source_file_sha256': sources, 'input_binary_sha256': executables,
        'source_columns': {'complete_source_cells': 7, 'inferred_source_cells': 15, 'hardware_measured': 0},
        'routing_probe_sha256': sha(routing_source.read_bytes()),
        'a30_probe_reused_without_edits': True,
        'new_core_probe_sha256': sha((args.raw / 'review-probe').read_bytes()),
        'compiled_routing_probe_sha256': sha((args.raw / 'material-routing').read_bytes()),
        'fresh_runtime': ['all22 soft and firm captures', 'all28 rank/selector body gates and all22 tab helper resets',
                          'source-factory nominal suite', '6 existing-library stack measurements'],
        'native_host_gui_executed': False, 'physical_hardware_measurement': False,
        'source_6_custom_probe_used': False, 'commands': commands,
        'evidence_sha256': {p.name: sha(p.read_bytes()) for p in sorted(OUT.iterdir()) if p.is_file() and p.name != 'receipt.json'}})
    print(json.dumps({'programs': len(rows), 'unchanged_values': 22 * 204, 'source_columns': len(columns),
                      'fresh_single_note_captures': len(touches) * 2, 'fresh_stacks': len(stacks),
                      'material_routing_checks': 442, 'source_nominal_checks': 378861,
                      'effort_early_contrast_db': [min(effort), max(effort)]}, indent=2))


if __name__ == '__main__':
    main()
