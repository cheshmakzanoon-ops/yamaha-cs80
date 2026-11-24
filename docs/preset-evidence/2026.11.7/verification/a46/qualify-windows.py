#!/usr/bin/env python3
"""Compose already completed, immutable A46 Windows attempt-one evidence.

This is an offline composition receipt, not a new native/validator execution.
Authentication is supplied by the retained root acquisition receipts. No old
helper result is modified or promoted; only a fresh platform result is written.
"""
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN = 45397189410
ATTEMPT = 1
PRODUCER = 138455976256
CONSUMER = 138483772246
REVISION = '2026.11.7'
TARGET = 'windows-x86_64'
ROOT_RECEIPT = 'b914626a64b3dec3305a0228540f4976d908570741bc6b131dcd6236e8fd9938'
PRODUCER_RECEIPT = '5edb6a9a9c2531f0682f0f3ef9567bf02320b2d13d28721c5a00b6324728d868'
PHASE1_MANIFEST = '10a3ff4fcec0984e8b05101cc46f8a7e0dea70b54a5b13a819ed0af053d407f8'
REPORT_PINS = '84bbe18a619a72cf5dda8e1607a57d421751d270037b09714112c1affd49ac8c'
ARTIFACTS = {
    'native': (14926075048, 84062, '5f1a1a5b5d3ff1a79640978d1d896d8a7736afe8ee4184a24b170ae329f0ee97'),
    'validator': (14925398286, 5082, '28a102fce6fee3c7713c6ed96380d4b4c86c910593fbd0b10e7930facdd342e2'),
    'vst3': (14924634293, 3205643, 'd9a8a318c409ed19c9a85deab7183f15bc3ea751c93bb52e9a292e008aa7d9b9'),
    'install': (14925251949, 6409200, '8f8f885c72910a0af02055036203a6a2e0aa96a9390adce7c1ec480243ba0feb'),
}
COUNTS = {'registered': 255, 'passed': 253, 'failed': 0, 'formal_skipped': 2, 'disabled': 0, 'unknown': 0}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    base = repo / 'docs/preset-evidence/2026.11.7'
    ci = base / 'ci/source-ee0dabca/windows-x86_64'
    prep = base / 'verification/a46'
    require(not args.out.exists(), 'Refusing to overwrite any existing result')

    def path(value):
        p = Path(value)
        return p if p.is_absolute() else repo / p

    def record(value):
        p = path(value)
        data = p.read_bytes()
        return {'path': str(p), 'bytes': len(data), 'sha256': sha(data)}

    def check_record(obj):
        actual = record(obj['path'])
        require(actual['bytes'] == obj['bytes'] and actual['sha256'] == obj['sha256'], f'Changed input: {obj["path"]}')
        return actual

    def read(p, digest=None):
        data = path(p).read_bytes()
        if digest:
            require(sha(data) == digest, f'Pinned receipt changed: {p}')
        return json.loads(data)

    def source_bytes(name):
        return subprocess.run(['git', '-C', str(repo), 'show', f'{SOURCE}:{name}'], check=True, capture_output=True).stdout

    def context(obj):
        obj = obj.get('provenance', obj)
        require(obj.get('source') == SOURCE and obj.get('run_id') == RUN, 'Source/run mismatch')
        require(obj.get('attempt', obj.get('attempt_context', obj.get('run_attempt'))) == ATTEMPT, 'Attempt mismatch')
        for key, expected in [('tree', TREE), ('source_tree', TREE), ('factory_revision', REVISION), ('revision', REVISION), ('target', TARGET)]:
            if key in obj:
                require(obj[key] == expected, f'Context mismatch: {key}')

    tree = subprocess.run(['git', '-C', str(repo), 'rev-parse', f'{SOURCE}^{{tree}}'], check=True, capture_output=True, text=True).stdout.strip()
    require(tree == TREE, 'Frozen source tree mismatch')
    phase1 = read(prep / 'retained-files.json', PHASE1_MANIFEST)
    require(len(phase1['files']) == 20, 'Phase-one retained count mismatch')
    preserved_phase1 = [check_record(item) for item in phase1['files']]
    old = read(prep / 'preserved-inputs-before.json')
    require(len(old) == 13, 'Historical protected input count mismatch')
    preserved_old = [check_record(item) for item in old]
    contract = read(prep / 'prepared-contract.json')
    context(contract)
    require(contract['platform_qualified'] is False and contract['actual_artifacts_checked'] is False, 'Prepared result must remain unqualified')
    helpers = [check_record(item) for item in contract['helper_pins']]
    for item in contract['source_pins']:
        data = source_bytes(item['path'])
        require(len(data) == item['bytes'] and sha(data) == item['sha256'], f'Frozen source pin mismatch: {item["path"]}')

    retrieval = read(ci / 'retrieval-receipt.json', ROOT_RECEIPT)
    producer_retrieval = read(ci / 'producer-retrieval-receipt.json', PRODUCER_RECEIPT)
    require(retrieval['source_commit'] == SOURCE and retrieval['run_id'] == RUN and retrieval['attempt'] == ATTEMPT, 'Retrieval identity mismatch')
    require(retrieval['producer_job_id'] == PRODUCER and retrieval['consumer_job_id'] == CONSUMER, 'Retrieval jobs mismatch')
    require(retrieval['prior_producer_receipt']['sha256'] == PRODUCER_RECEIPT and retrieval['prior_producer_receipt']['all_original_inputs_preserved'] is True, 'Original producer receipt changed')
    require(len(retrieval['files']) == 16 and len(retrieval['archives']) == 4, 'Acquisition incomplete')
    raw_inputs = [check_record({'path': str(ci / name), **identity}) for name, identity in retrieval['files'].items()]
    for name, identity in producer_retrieval['files'].items():
        require(retrieval['files'][name] == identity, f'Initial raw input changed: {name}')
    archives = {}
    for item in retrieval['archives']:
        identity = check_record({'path': item['local_path'], 'bytes': item['bytes'], 'sha256': item['sha256']})
        require((item['artifact_id'], item['bytes'], item['sha256']) == ARTIFACTS[item['kind']], 'Acquired archive identity mismatch')
        require(item['api_identity_matched'] is True, 'Missing acquisition API binding')
        archives[item['kind']] = {'artifact_id': item['artifact_id'], **identity}
    require(set(archives) == set(ARTIFACTS), 'Missing artifact role')

    pins = read(ci / 'a46-actual-report-pins.json', REPORT_PINS)
    require(len(pins['files']) == 16, 'Actual evidence inventory changed')
    result_inputs = [check_record(item) for item in pins['files']]
    reports = {Path(item['path']).name: read(item['path']) for item in pins['files']}
    integrity = reports['a46-input-integrity.json']
    require(integrity['phase1_all_20_retained_files_unchanged'] and integrity['phase1_helper_pins_unchanged'], 'Initial preservation check failed')
    receipts = reports['a46-initial-command-receipts.json'] + reports['a46-final-command-receipts.json']
    commands = read(prep / 'acceptance-commands.json')['commands']
    expected_commands = {item['label']: item['argv'] for item in commands}
    require(len(receipts) == len(expected_commands) == 7 and len({item['label'] for item in receipts}) == 7, 'Seven checker commands required')
    for item in receipts:
        require(item['exit_code'] == 0 and item['cwd'] == str(repo), 'Checker did not complete normally')
        require(item['argv'] == expected_commands[item['label']], 'Actual command differs from prepared command')
        check_record(item['checker'])
        check_record(item['log'])
    for item in reports.values():
        if isinstance(item, dict) and 'platform_qualified' in item:
            require(item['platform_qualified'] is False, 'Subordinate receipt must retain its bounded qualification')
        if isinstance(item, dict) and 'release_qualified' in item:
            require(item['release_qualified'] is False, 'Subordinate release qualification changed')

    obs = reports['native-validator-observations.json']
    binding = reports['parser-archive-binding.json']
    native_contract = reports['native-content-contract.json']
    original = reports['original-ci-binding.json']
    install = reports['install-acceptance.json']
    payload = reports['payload-binding.json']
    docs = reports['docs-and-notices.json']
    detail = reports['a46-native-detail-review.json']
    skips = reports['skip-review.json']
    validator_review = reports['a46-validator-review.json']
    package = reports['a46-package-producer-proof.json']
    for item in [binding, native_contract, original, install, payload, docs, detail, skips, validator_review, package]:
        context(item)
    for item in binding['inputs'].values():
        check_record(item)
    for item in original['input_records'].values():
        check_record(item)
    for item, keys in [(docs, ['acceptance_receipt']), (detail, ['skip_review', 'original_ci_binding', 'native_archive', 'observations']), (skips, ['observations', 'native_archive']), (validator_review, ['original_ci_binding', 'observations']), (package, ['producer_log', 'install_acceptance', 'original_ci_binding'])]:
        for key in keys:
            check_record(item[key])
    require(binding['parser_archive_binding_verified'] is True, 'Parser/member binding failed')
    require(obs['all_requested_observations_green'] is True and len(obs['native']) == len(native_contract['native']) == 1, 'Native observation incomplete')
    native = obs['native'][0]
    native_gate = native_contract['native'][0]
    require(native['counts'] == native_gate['counts'] == detail['counts'] == COUNTS, 'Native result counts mismatch')
    cases = native['testcases']
    require(len(cases) == 255 and sorted(item['name'] for item in cases) == contract['native_expected']['all_names'], 'Full 255-name roster mismatch')
    require(detail['exact_full_roster_matches'] and native_gate['native_content_contract_verified'] and native_gate['ui_body_identity_phase_complete'], 'Native content gate failed')
    require(native_gate['skip_review_sha256'] == record(ci / 'skip-review.json')['sha256'], 'Skip review not bound')
    require(skips['reviewed'] is True and len(skips['formal_reviews']) == 2 and all(item['reviewed'] for item in skips['formal_reviews']), 'Formal skips not individually reviewed')
    require(sorted(item['name'] for item in skips['formal_reviews']) == contract['native_expected']['required_formal_skip_names'], 'Wrong formal skip names')
    require(skips['formal_skips'] == native['formal_skips'] == native_gate['formal_skips'], 'Formal skip observations disagree')
    require(skips['nested_python_skips'] == native['nested_python_skips'] == native_gate['nested_python_skips'], 'Nested skip observations disagree')
    require(skips['nested_python_skips']['total'] == 11 and len(skips['nested_reviews']) == 8 and all(item['reviewed'] for item in skips['nested_reviews']), 'Nested skips not separately reviewed')
    require(sum(item['observed_skips'] for item in skips['nested_reviews']) == 11, 'Nested skip count changed')
    require({item['name'] for item in detail['new_tests']} == set(contract['native_expected']['required_new_tests']) and all(item['result'] == 'passed' for item in detail['new_tests']), 'New native gates failed')
    parity = detail['parity_capture']
    require(parity['result']['result'] == 'passed' and parity['harness_test_count'] == 13 and sum(map(len, parity['source_methods'].values())) == 13, 'Repaired parity harness incomplete')
    require(parity['zero_failures_errors_or_skips'] and parity['actual_live_capture_cases_included'] and parity['harness_seconds'] == 4.026 and parity['ctest_seconds'] == 4.10695, 'Parity actual result mismatch')
    require(detail['ui_body_identity']['complete_lasttest_used'] and detail['ui_body_identity']['result']['result'] == 'passed' and detail['library_audit']['result']['result'] == 'passed', 'Complete UI/library evidence failed')

    require(original['producer_job_id'] == PRODUCER and original['consumer_job_id'] == CONSUMER, 'Original jobs mismatch')
    require(original['original_ctest_exit_zero_verified'] and original['numeric_ctest_exit_directly_logged'] is False, 'Original CTest exit basis incomplete')
    require(original['original_clean_checkout_juce_build_test_package_sequence_verified'] is True, 'Original source/build/package sequence failed')
    require(len(original['original_required_producer_steps']) == 9 and len(original['original_required_consumer_steps']) == 6, 'Original required step coverage incomplete')
    require(all(item['conclusion'] == 'success' for item in original['original_required_producer_steps'] + original['original_required_consumer_steps']), 'Original required step unsuccessful')
    require(all(item['exact_source_commands_verified'] for item in original['command_bindings'].values()), 'Original commands not source-bound')
    for role, item in original['artifact_bindings'].items():
        require((item['artifact_id'], item['bytes'], item['sha256']) == ARTIFACTS[role], 'Original upload differs from acquired artifact')
        require(item['producing_attempt'] == ATTEMPT and item['producing_job_id'] == (CONSUMER if role == 'validator' else PRODUCER), 'Artifact producing attempt/job mismatch')
        require(item['original_upload_id_size_digest_and_time_verified'] is True, 'Original upload not bound')
    download = original['consumer_download_binding']
    require((download['artifact_id'], download['bytes'], download['sha256']) == ARTIFACTS['vst3'], 'Consumer did not download the checked product')
    require(download['consumer_job_id'] == CONSUMER and download['attempt'] == 1 and download['original_invocations'] == 2 and download['independent_of_compiler_sidecar_presence'], 'Independent consumer binding missing')

    vals = original['validators']
    require(vals['required_lanes'] == vals['actual_invocations'] == len(vals['lanes']) == 2 and vals['validation_retries'] == 0, 'Wrong validator invocation/retry count')
    require(validator_review['total_lanes'] == validator_review['actual_invocations'] == 2 and validator_review['validation_retries'] == 0, 'Validator review count mismatch')
    observed_lanes = {item['required_workflow_step']: item for item in obs['validators'][0]['lanes']}
    for lane in vals['lanes']:
        summary = lane['summary']
        require(summary['strictness'] == [10] and summary['program_counts'] == [523] and summary['validation_start_count'] == summary['success_lines'] == 1 and summary['final_nonempty_line'] == 'SUCCESS', 'Validator content mismatch')
        require(summary['warnings'] == summary['failure_markers'] == [] and lane['original_normal_exit'] == 0 and lane['numeric_child_exit_directly_logged'] is False, 'Validator warnings/failure/exit mismatch')
        require(lane['console_matches_original_runtime'] and lane['console_report_equivalent'] and lane['invocations'] == 1 and lane['validation_retries'] == 0, 'Console/report/original binding failed')
        observed = observed_lanes[lane['original_step']['name']]
        for role in ['console', 'report']:
            require(all(lane[role][key] == observed[role][key] for key in ['path', 'bytes', 'sha256']), 'Observed lane member mismatch')
    require([item['summary']['seeds'] for item in vals['lanes']] == [['0x3e82dcb'], ['0x5037a0c']], 'Actual random/fixed seeds mismatch')
    require(all(item['reviewed'] and len(item['explicit_skips']) == 1 and item['explicit_skips'][0]['reviewed'] for item in validator_review['lanes']), 'Validator informational skips unreviewed')
    require(vals['pinned_download'] == obs['validators'][0]['version']['download'] and vals['pinned_download']['verified'] and len(vals['pinned_download']['attempts']) == 1, 'Official pinned tool provenance mismatch')

    require(install['status'] == 'artifact_bytes_verified' and install['source_tree'] == TREE and install['preset_library_revision'] == REVISION and install['inventory_entries'] == 20, 'Install acceptance failed')
    require(install['artifact']['id'] == ARTIFACTS['install'][0] and install['outer_zip']['sha256'] == ARTIFACTS['install'][2], 'Install API/outer identity mismatch')
    require(len(install['outer_zip']['members']) == 3 and install['formats'] == ['VST3', 'Standalone'], 'Install archive structure mismatch')
    require(len(install['binary_headers']) == 2 and all(item['header_check'] == 'PASS' and item['header']['format'] == 'PE32+' and item['header']['machine'] == '0x8664' for item in install['binary_headers']), 'Windows image role/header check failed')
    require(docs['artifact_id'] == ARTIFACTS['install'][0] and docs['outer_zip']['sha256'] == ARTIFACTS['install'][2] and docs['inner_archive']['sha256'] == install['inner_archive']['sha256'], 'Docs inspected another archive')
    require(docs['inventory_counts'] == {'file': 16, 'directory': 4} and len(docs['required_metadata']) == 13 and all(item['type'] == 'file' and item['mode'] == '0o644' and item['size'] > 0 for item in docs['required_metadata']), 'Metadata contract failed')
    require(len(docs['guide_comparisons']) == 3 and all(item['result'] == 'PASS' for item in docs['guide_comparisons']), 'Exact source guide comparison failed')
    require(docs['common_notice_mode'] == 'all_complete_lf_to_crlf_checkout_bytes' and len(docs['notice_comparisons']) == 7 and all(item['result'] == 'PASS' and item['comparison_mode'] == docs['common_notice_mode'] and item['actual_raw'] == item['controlled_checkout'] for item in docs['notice_comparisons']), 'All-seven notice common mode failed')
    require(docs['source_checkout_proof']['scoped_clean_diff_exit'] == 0 and docs['source_checkout_proof']['shared_index_or_refs_modified'] is False, 'Controlled checkout proof failed')
    require(package['original_package_json']['sha256'] == package['actual_inner_archive']['sha256'] == install['inner_archive']['sha256'] and package['original_printed_inner_hash_matches_actual'], 'Original package inner archive binding failed')
    require(package['native_and_packaged_juce_provenance_bytes_equal'] and package['original_clean_checkout_juce_build_test_package_sequence_verified'], 'JUCE license production chain missing')
    require(package['juce_provenance']['sha256'] == original['juce_provenance']['sha256'] and package['juce_license'] == docs['juce_license'], 'JUCE provenance/license binding differs')
    require(package['independent_juce_checkout_byte_comparison'] is False and docs['juce_license']['independent_dependency_source_byte_comparison'] is False, 'Independent JUCE comparison overclaimed')
    require(payload['status'] == 'distributed_vst3_matches_validator_input' and payload['installation_artifact']['artifact_id'] == ARTIFACTS['install'][0] and payload['validator_input_artifact']['artifact_id'] == ARTIFACTS['vst3'][0], 'Product/report artifact confusion')
    require(payload['validator_input_artifact']['sha256'] == ARTIFACTS['vst3'][2] and payload['installation_artifact']['sha256'] == ARTIFACTS['install'][2], 'Payload artifact bytes differ')
    require(payload['payload_entries'] == 6 and payload['files'] == 2 and payload['symlinks'] == 0 and payload['file_bytes'] == 7394935, 'Complete selected payload counts mismatch')
    require(payload['payload_tree_sha256'] == '155eff8cb5857fc6c513990755a5657efd11ca6e941952dd1c5a66458741b1e5', 'Selected VST3 tree mismatch')
    require([item['path'] for item in payload['validator_container_sidecars']] == ['CS-80 Reference.exp', 'CS-80 Reference.lib'], 'Unexpected compiler container exception')
    require([item['format_check']['format'] for item in payload['validator_container_sidecars']] == ['AMD64 COFF object', 'COFF import archive'] and all(item['type'] == 'file' and item['included_in_vst3_payload_comparison'] is False for item in payload['validator_container_sidecars']), 'Compiler sidecar format/position exception failed')
    require(payload['validator_container_contract']['consumer_log']['sha256'] == record(ci / 'validator-job.log')['sha256'] and payload['validator_container_contract']['consumer_log']['started_validation_paths'] == [download['selected_bundle_path']] * 2, 'Payload selection not same original consumer')

    gate_sources = {
        'original_identity': ['retrieval-receipt.json', 'original-ci-binding.json'],
        'producer_success': ['original-ci-binding.json'],
        'native_names_and_results': ['native-validator-observations.json', 'parser-archive-binding.json', 'native-content-contract.json'],
        'native_skips': ['skip-review.json', 'native-content-contract.json'],
        'repaired_parity_capture': ['a46-native-detail-review.json'],
        'new_gates_and_ui': ['native-content-contract.json', 'a46-native-detail-review.json'],
        'ctest_original_exit': ['original-ci-binding.json'],
        'artifact_uploads': ['retrieval-receipt.json', 'original-ci-binding.json'],
        'pluginval_pin': ['original-ci-binding.json'],
        'two_original_lanes': ['original-ci-binding.json', 'a46-validator-review.json'],
        'console_original_binding': ['original-ci-binding.json', 'parser-archive-binding.json'],
        'consumer_download': ['original-ci-binding.json'],
        'installation': ['install-acceptance.json', 'a46-package-producer-proof.json'],
        'documentation': ['docs-and-notices.json', 'a46-package-producer-proof.json'],
        'juce_license_chain': ['original-ci-binding.json', 'a46-package-producer-proof.json'],
        'complete_vst3_equality': ['payload-binding.json', 'original-ci-binding.json'],
        'composition': ['a46-actual-report-pins.json', 'a46-initial-command-receipts.json', 'a46-final-command-receipts.json'],
    }
    require(set(gate_sources) == {item['gate'] for item in contract['required_gates']}, 'Prepared gate coverage incomplete')
    gates = [{**item, 'actual_verified': True, 'evidence': [record(ci / name) for name in gate_sources[item['gate']]]} for item in contract['required_gates']]
    result = {
        'schema': 1, 'status': 'windows_original_attempt_platform_qualified',
        'qualified_at_utc': datetime.now(timezone.utc).isoformat(),
        'factory_revision': REVISION, 'source': SOURCE, 'tree': TREE, 'run_id': RUN, 'attempt': ATTEMPT, 'target': TARGET,
        'producer_job_id': PRODUCER, 'consumer_job_id': CONSUMER,
        'native_results_accepted': True, 'original_attempt_binding_verified': True,
        'original_ctest_exit_zero_verified': True, 'original_validator_exits_zero_verified': True,
        'platform_qualified': True, 'release_qualified': False, 'global_qualified': False,
        'scope': 'Only original Windows x86_64 producer and validator consumer from frozen ee0 source, run 45397189410, attempt 1. No other platform or later attempt is adopted.',
        'acquisition': {'authority': retrieval['transport'], 'root_receipt': record(ci / 'retrieval-receipt.json'), 'initial_producer_receipt': record(ci / 'producer-retrieval-receipt.json'), 'raw_inputs': raw_inputs, 'archives': archives, 'offline_helper_independently_authenticated_service': False, 'boundary': 'Authentication derives from root-authenticated repository-owner connector acquisition supplied in the task. This composition independently rechecks bytes, source/run/attempt/job/upload/consumer bindings; it does not authenticate arbitrary supplied JSON.'},
        'required_gates': gates,
        'original_jobs': {key: original[key] for key in ['producer_checkout', 'consumer_checkout', 'original_required_producer_steps', 'original_required_consumer_steps', 'source_workflow', 'command_bindings', 'ctest_exit_basis', 'numeric_ctest_exit_directly_logged', 'timestamp_tolerance', 'log_representation']},
        'artifact_bindings': original['artifact_bindings'],
        'consumer_download_binding': download,
        'native': {'counts': COUNTS, 'exact_registered_names': contract['native_expected']['all_names'], 'formal_skips': skips['formal_skips'], 'formal_skip_reviews': skips['formal_reviews'], 'nested_python_skips': skips['nested_python_skips'], 'nested_skip_review': record(ci / 'skip-review.json'), 'new_tests': detail['new_tests'], 'parity_capture': parity, 'ui_body_identity': detail['ui_body_identity'], 'library_audit': detail['library_audit'], 'parsed_member_records': native_gate['parsed_member_records'], 'counts_scope': '253 passed plus two formal capability skips equals 255 CTest cases. Eleven nested Python skips across eight passing CTest suites are counted separately.'},
        'validators': {**vals, 'review': record(ci / 'a46-validator-review.json'), 'explicit_skip_reviews': validator_review['lanes'], 'independent_vst3_sdk_validator_executed': False, 'windows_pluginval_gui_lane_executed': False},
        'installation': {key: install[key] for key in ['artifact', 'outer_zip', 'inner_archive', 'manifest_sha256', 'checksum_manifest_sha256', 'inventory_entries', 'inventory', 'formats', 'binary_headers', 'source_contract_files']},
        'complete_vst3_equality': {key: payload[key] for key in ['installation_artifact', 'validator_input_artifact', 'payload_tree_sha256', 'payload_entries', 'files', 'symlinks', 'file_bytes', 'members', 'validator_container_sidecars', 'validator_container_contract', 'scope']},
        'documentation': {key: docs[key] for key in ['required_metadata', 'guide_comparisons', 'notice_comparisons', 'common_notice_mode', 'source_checkout_proof', 'normalization_scope', 'source_time_guides']},
        'juce': {'original_clean_pin_and_sequence_verified': True, 'provenance': original['juce_provenance'], 'license': docs['juce_license'], 'package_proof': record(ci / 'a46-package-producer-proof.json'), 'native_and_packaged_provenance_bytes_equal': True, 'independent_dependency_checkout_byte_comparison': False},
        'actual_commands': receipts,
        'input_report_identities': result_inputs,
        'source_pins': contract['source_pins'], 'helper_pins': helpers,
        'preservation': {'phase1_manifest': record(prep / 'retained-files.json'), 'all_20_phase1_files_unchanged': True, 'phase1_files': preserved_phase1, 'all_13_protected_historical_inputs_unchanged': True, 'historical_inputs': preserved_old, 'c554_original_failure_remains_failure': True, 'c554_failure_context': detail['prior_failure'], 'raw_root_api_logs_archives_modified': False, 'old_helpers_or_receipts_modified': False},
        'limitations': [
            'Release/global qualification remains false. Other platforms, later attempts, the complete fifteen-lane external matrix, and their original outcomes are outside this Windows receipt.',
            'Original normal-zero CTest/pluginval exits follow exact unsuppressed source commands and successful original API steps; numeric child exit codes are not directly printed. SUCCESS text alone is insufficient.',
            'All formal capability skips and nested Python skips remain skips. SIMD and Linux/GCC-only deterministic coverage are not claimed on MSVC.',
            'Both pluginval lanes are headless. The independent Steinberg VST3 SDK validator was not configured; its explicit informational skip is reviewed. Native UI smoke is separately verified.',
            'Controlled Windows checkout demonstrates the exact all-seven notice LF-to-CRLF transform. Actual CI git EOL settings were not printed; no broader normalization is accepted.',
            'JUCE license acceptance uses raw package inventory plus original clean JUCE 8.0.11 c352e248 configure/build/package chain. An independent dependency-checkout license-byte comparison was not performed.',
            'No release publication, unauthenticated public download GET, release signing/notarization, real machine installation, standalone launch, DAW session, human listening, hardware/reference audition, or acoustic fidelity claim is established.',
            'Packaged guides are exact source-time bytes and retain their historical/pending statements. This result does not rewrite those guides or imply their publication claims changed.',
        ],
        'checker': record(Path(__file__).resolve()),
    }
    with args.out.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'report': record(args.out), 'verified_gates': len(gates), 'native_counts': COUNTS, 'validator_lanes': 2, 'platform_qualified': True, 'release_qualified': False, 'global_qualified': False}, indent=2))


if __name__ == '__main__':
    main()
