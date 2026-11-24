#!/usr/bin/env python3
"""Reproduce the frozen .5-to-.6 test/gate source audit; no products executed."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[5]
OLD = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
NEW = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'

def source(commit, path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + path])

def digest(data):
    return hashlib.sha256(data).hexdigest()

def require(ok, message):
    if not ok:
        raise ValueError(message)

def step(text, name):
    marker = '      - name: ' + name + '\n'
    require(text.count(marker) == 1, 'Missing/ambiguous workflow step: ' + name)
    return text.split(marker, 1)[1].split('\n      - ', 1)[0]

def run_body(block):
    lines = block.split('        run: |\n', 1)[1].splitlines()
    body = []
    for line in lines:
        if line.strip() and not line.startswith('          '):
            break
        body.append(line)
    return '\n'.join(body).rstrip() + '\n'

def main():
    result = {'baseline': OLD, 'candidate': NEW, 'scope': 'Static source audit; no new audio/native execution',
              'unchanged_files': {}, 'inputs': {}, 'new_registrations': {}}
    paths = ['tests/MalletPresetRefinementTests.cpp', 'tests/BassPluckedRefinementTests.cpp',
             'src/core/CuratedLibraryLevels.inc', 'scripts/compare-preset-response.py']
    for path in paths:
        before, after = source(OLD, path), source(NEW, path)
        require(before == after, 'Previously frozen gate changed: ' + path)
        result['unchanged_files'][path] = {'sha256': digest(after), 'bytes': len(after)}

    path = 'tests/LibraryAuditTests.cpp'
    before, after = source(OLD, path), source(NEW, path)
    expected = before.replace(b'|| i == P_body1Variant || i == P_body2Variant;',
                              b'|| i == P_body1Variant || i == P_body2Variant || i == P_body1Material || i == P_body2Material;')
    require(expected == after and before != after, 'LibraryAudit changed beyond discrete selector coverage')
    result['library_audit_change'] = 'Only the two appended Material selectors join the integer-value classifier'

    for path in ['CMakeLists.txt', '.github/workflows/build.yml', 'tests/LibraryAuditTests.cpp',
                 'tests/PresetCharacterTests.cpp', 'tests/PianoCourseGeometryTests.cpp',
                 'tests/WindBodyFormantTests.cpp', 'tests/MaterialBodyTests.cpp',
                 'tests/PluckedMaterialTests.cpp', 'tests/BellMaterialPresetTests.cpp',
                 'tests/PluckedMaterialPresetTests.cpp', 'tests/InstrumentBodyHostTests.h',
                 'scripts/compare-preset-response-2026-11-6.py',
                 'scripts/test-compare-preset-response-2026-11-6.py',
                 'scripts/capture-factory-auditions.py', 'scripts/test-capture-factory-auditions.py',
                 'scripts/test-pluginval-macos-download.py', 'scripts/test-pluginval-macos-download-negative.py']:
        data = source(NEW, path)
        require((ROOT / path).read_bytes() == data, 'Inspected production/test input changed after candidate: ' + path)
        result['inputs'][path] = {'sha256': digest(data), 'bytes': len(data)}

    cmake = source(NEW, 'CMakeLists.txt').decode()
    names = ['cs80_piano_course_geometry_tests', 'cs80_wind_body_formant_tests',
             'cs80_material_body_tests', 'cs80_plucked_material_tests',
             'cs80_bell_material_preset_tests', 'cs80_plucked_material_preset_tests',
             'cs80_preset_response_2026_11_6_comparison_tests']
    stack = []
    for line in cmake.splitlines():
        match = re.match(r'\s*if\s*\((.*)', line, re.I)
        if match:
            stack.append(match.group(1).rstrip(')'))
        for name in names:
            if re.search(r'add_test\(NAME\s+' + name + r'\b', line):
                require(name not in result['new_registrations'], 'Duplicate test registration: ' + name)
                require(stack == ['CS80_BUILD_TESTS'], 'Platform-restricted new test: ' + name + repr(stack))
                result['new_registrations'][name] = {'conditional_scope': list(stack)}
        if re.match(r'\s*endif\s*\(', line, re.I):
            stack.pop()
    require(len(result['new_registrations']) == 7, 'Missing new CTest registration')
    for name in names:
        match = re.search(r'set_tests_properties\(' + name + r' PROPERTIES TIMEOUT (\d+)\)', cmake)
        require(match is not None, 'Missing bounded timeout: ' + name)
        result['new_registrations'][name]['timeout_seconds'] = int(match.group(1))
    for name in names[4:6]:
        require(f'target_link_libraries({name} PRIVATE cs80_core)' in cmake,
                'Complete-preset test does not link Engine library: ' + name)

    before = source(OLD, '.github/workflows/build.yml').decode()
    after = source(NEW, '.github/workflows/build.yml').decode()
    old_lanes = ['Validate VST3 with pluginval', 'Validate AU with pluginval',
                 'Validate AU with pluginval GUI', 'Validate AU GUI fixed-seed regression']
    result['unchanged_mac_command_bodies'] = {}
    for name in old_lanes:
        a, b = run_body(step(before, name)), run_body(step(after, name))
        require(a == b, 'Existing validator command body changed: ' + name)
        result['unchanged_mac_command_bodies'][name] = digest(b.encode())
    new_name = 'Validate AU GUI regression seed 0x3c99748'
    new_lane = step(after, new_name)
    for token in ['--strictness-level 10', '--validate-in-process', '--random-seed 0x3c99748',
                  '--timeout-ms 120000', 'set -euo pipefail', "grep -qx 'SUCCESS' pluginval-au-seed-c76-regression.log"]:
        require(token in new_lane, 'Missing new-lane contract: ' + token)
    require('--skip-gui-tests' not in new_lane and 'continue-on-error' not in new_lane
            and '|| true' not in new_lane, 'New GUI lane weakens outcome propagation')
    condition = "if: always() && !cancelled() && (steps.validate_au_gui.outcome == 'success' || steps.validate_au_gui.outcome == 'failure')"
    for name in [old_lanes[-1], new_name]:
        require(condition in step(after, name), 'Fixed seed can be skipped after a failed random GUI lane')
    package = step(after, 'Package native macOS VST3, AU and Standalone')
    require('        if:' not in package and 'continue-on-error' not in package,
            'Packaging overrides default preceding-success gate')
    result['mac_gates'] = {'lanes_per_mac': 5, 'previous_four_commands_unchanged': True,
                           'fixed_lanes_run_after_attempted_random_gui': True,
                           'new_lane_failure_not_ignored': True, 'packaging_has_no_status_override': True}

    character = source(NEW, 'tests/PresetCharacterTests.cpp').decode()
    for token in ['constexpr double minimumBodyRms = 0.001;', 'check(error < 0.01',
                  'check(onset < 0.5', 'check(late < 0.25 * onset', 'materialContact();']:
        require(token in character, 'Restored character contract absent: ' + token)
    require('Case { "Harp", 60, 0.08 }' not in character and 'Case { "Kalimba", 60, 0.08 }' not in character,
            'Obsolete accent expectation unexpectedly remains')
    result['character_change'] = 'Retire only the 8% unrelated-accent minimum for Harp/Kalimba; restore reconstruction, body floors, contact ceiling and relative decay'
    result['audition_budget_change'] = {'old_raw_audio_budget': 480000000,
                                        'new_raw_audio_budget': 500000000,
                                        'configured_raw_audio_bytes': 490060800,
                                        'scope': 'Fixture storage budget, not an audio quality or clipping limit'}
    result['status'] = 'source-audit-passed'
    output = Path(__file__).with_name('source-audit.json')
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'unchanged_files': len(paths),
                      'new_cross_platform_registrations': len(result['new_registrations']),
                      'candidate_inputs_matched': len(result['inputs'])}, sort_keys=True))

if __name__ == '__main__':
    main()
