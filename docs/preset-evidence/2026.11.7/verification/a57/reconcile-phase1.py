#!/usr/bin/env python3
"""Reconcile A57's first two actual packages with original upload/public records.

This is deliberately a two-of-four checkpoint. It records external ARM
qualification without transferring that status to A57 or another platform.
"""
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[5]
BASE = ROOT / 'docs/preset-evidence/2026.11.7'
OWN = BASE / 'verification/a57'
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN, ATTEMPT, REVISION = 45397189410, 1, '2026.11.7'
ROWS = {
    'macos-arm64': dict(platform='macos', architecture='arm64', artifact_name='cs80-macos-15-install-bundle',
                        inner='cs80-reference-macos-arm64-ee0dabcacb4c.tar.gz', required_native_payloads=3),
    'macos-x86_64': dict(platform='macos', architecture='x86_64', artifact_name='cs80-macos-15-intel-install-bundle',
                         inner='cs80-reference-macos-x86_64-ee0dabcacb4c.tar.gz', required_native_payloads=3),
    'windows-x86_64': dict(platform='windows', architecture='x86_64', artifact_name='cs80-windows-install-bundle',
                           inner='cs80-reference-windows-x86_64-ee0dabcacb4c.zip', required_native_payloads=2),
    'linux-x86_64': dict(platform='linux', architecture='x86_64', artifact_name='cs80-linux-install-bundle',
                         inner='cs80-reference-linux-x86_64-ee0dabcacb4c.tar.gz', required_native_payloads=2),
}
ACTUAL = {
    'macos-arm64': dict(job=138455976227, acquisition='retrieval-receipt.json',
                        acquisition_sha256='1c8999da31d092f9bdb435eb3f76790e1f7314f040a397cd3676776d26a5ea6c',
                        package_step='Package native macOS VST3, AU and Standalone',
                        upload_step='Upload native macOS install bundle'),
    'windows-x86_64': dict(job=138455976256, acquisition='producer-retrieval-receipt.json',
                           acquisition_sha256='5edb6a9a9c2531f0682f0f3ef9567bf02320b2d13d28721c5a00b6324728d868',
                           package_step='Package Windows VST3 and Standalone',
                           upload_step='Upload Windows install bundle'),
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def ident(path):
    path = Path(path)
    with path.open('rb') as stream:
        return dict(path=str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
                    bytes=path.stat().st_size, sha256=hashlib.file_digest(stream, 'sha256').hexdigest())


def read(path):
    return json.loads(Path(path).read_bytes())


def instant(text):
    return datetime.fromisoformat(text.replace('Z', '+00:00'))


def step(job, name):
    matches = [s for s in job['steps'] if s['name'] == name]
    require(len(matches) == 1, 'Expected one original step: ' + name)
    result = matches[0]
    require(result['status'] == 'completed' and result['conclusion'] == 'success', 'Original step not successful')
    return result


def find_line(lines, text, exact=False):
    matches = [(number, stamp, content) for number, stamp, content in lines
               if (content == text if exact else text in content)]
    require(len(matches) == 1, 'Expected one original output line: ' + text)
    number, stamp, content = matches[0]
    return dict(line=number, timestamp=stamp, text=content)


def in_step(line, api_step):
    # GitHub API step endpoints have whole-second precision. Retain the raw
    # fractional original-log timestamp; permit only its completed second.
    return instant(api_step['started_at']) <= instant(line['timestamp']) < instant(api_step['completed_at']) + timedelta(seconds=1)


def main():
    require(subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', SOURCE + '^{tree}'], text=True).strip() == TREE,
            'Frozen source tree differs')
    output = dict(schema=1, status='two_of_four_actual_installation_packages_crosschecked',
                  source_commit=SOURCE, source_tree=TREE, factory_revision=REVISION,
                  run_id=RUN, run_attempt=ATTEMPT, verified_at_utc=datetime.now(timezone.utc).isoformat(),
                  installation_packages_crosschecked=2, required_installation_packages=4,
                  all_four_packages_reconciled=False, cross_platform_qualified=False, release_qualified=False,
                  public_downloads_reconciled=1, public_downloads_required_for_installation_matrix=4,
                  fifth_audition_download_is_external=True, targets={})
    for target, expectation in ROWS.items():
        row = dict(target=target, **expectation, package_status='pending_actual_handoff', artifact_id=None,
                   original_install_upload_attempt_bound=False, public_full_get_reconciled=False,
                   external_platform_qualification=None, platform_qualification_claimed_by_a57=False)
        output['targets'][target] = row
        if target not in ACTUAL:
            continue
        actual, ci, own = ACTUAL[target], BASE / 'ci/source-ee0dabca' / target, OWN / target
        summary, acceptance = read(own / 'crosscheck-summary.json'), read(own / 'install-acceptance.json')
        direct, docs = read(own / 'direct-archive-crosscheck.json'), read(own / 'docs-and-notices.json')
        require(summary['status'] == 'actual_package_crosscheck_passed'
                and summary['source_commit'] == SOURCE and summary['source_tree'] == TREE
                and summary['factory_revision'] == REVISION and summary['run_id'] == RUN
                and summary['run_attempt'] == ATTEMPT, 'A57 summary context differs')
        require(ident(ci / actual['acquisition'])['sha256'] == actual['acquisition_sha256'],
                'Root-supplied acquisition receipt pin differs')
        for path, expected in summary['input_files'].items():
            current = ident(path)
            require(all(current[key] == value for key, value in expected.items()), 'Original input changed')
        require(summary['artifact']['name'] == expectation['artifact_name']
                and summary['inner_archive']['file'] == expectation['inner'], 'Target package mapping differs')
        job, api = read(ci / 'producer-job-api.json'), read(ci / 'install-api.json')
        require(job['id'] == actual['job'] and job['head_sha'] == SOURCE and job['run_id'] == RUN
                and job['run_attempt'] == ATTEMPT and job['status'] == 'completed'
                and job['conclusion'] == 'success', 'Original producer identity/result differs')
        package, upload = step(job, actual['package_step']), step(job, actual['upload_step'])
        require(package['number'] < upload['number'], 'Upload precedes original packaging')
        lines, continuations = [], []
        for number, raw in enumerate((ci / 'producer-job.log').read_text().splitlines(), 1):
            if number == 1 and raw.startswith('\ufeff'):
                raw = raw[1:]
            match = re.match(r'^(\d{4}-\d\d-\d\dT\S+Z) (.*)$', raw)
            if match is None:
                continuations.append(dict(line=number, text=raw))
                continue
            lines.append((number, match[1], match[2]))
        finalize = find_line(lines, f"Artifact {api['name']}.zip successfully finalized. Artifact ID {api['id']}", True)
        success = find_line(lines, f"Artifact {api['name']} has been successfully uploaded! Final size is {api['size_in_bytes']} bytes. Artifact ID is {api['id']}", True)
        digest = find_line(lines, 'SHA256 digest of uploaded artifact zip is ' + acceptance['outer_zip']['sha256'], True)
        package_path = ('packages\\' if expectation['platform'] == 'windows' else 'packages/') + expectation['inner']
        archive = find_line(lines, '"archive": ' + json.dumps(package_path) + ',', False)
        inner_digest = find_line(lines, '"sha256": "' + acceptance['inner_archive']['sha256'] + '",', False)
        require(all(in_step(x, upload) for x in (finalize, success, digest)), 'Artifact output not in original successful upload step')
        require(all(in_step(x, package) for x in (archive, inner_digest)), 'Inner archive not in original successful package step')
        require(digest['line'] < finalize['line'] < success['line'], 'Original upload output order differs')
        require(all(line['line'] > success['line'] for line in continuations),
                'Untimestamped continuation precedes selected installation upload completion')
        require(instant(finalize['timestamp']).replace(microsecond=0) == instant(api['created_at']),
                'Original finalization/API creation second differs')
        require(api['expired'] is False and instant(api['expires_at']) > datetime.now(timezone.utc), 'Artifact currently expired')
        row.update(package_status='actual_package_crosscheck_passed', artifact_id=api['id'],
                   outer_zip=summary['outer_zip'], inner_archive=summary['inner_archive'],
                   api_created_at=api['created_at'], api_expires_at=api['expires_at'], api_expired=api['expired'],
                   inventory_counts=direct['inventory_counts'], inventory_entries=direct['inventory_entries'],
                   required_metadata_files=len(docs['required_metadata']), exact_source_rewritten_guides=3,
                   repository_notices=7,
                   notice_mode=docs.get('common_notice_mode', 'all_raw_source_bytes'),
                   binary_headers=direct['binary_headers'], original_install_upload_attempt_bound=True,
                   original_job_id=job['id'], original_package_step=package, original_upload_step=upload,
                   original_log_excerpts=[archive, inner_digest, digest, finalize, success],
                   later_log_continuations=continuations,
                   original_binding_scope='Exact installation artifact only; does not accept native tests or validators.',
                   input_receipts=[ident(own / name) for name in ('crosscheck-summary.json', 'install-acceptance.json',
                                                                 'docs-and-notices.json', 'direct-archive-crosscheck.json')],
                   original_inputs=[ident(ci / name) for name in (actual['acquisition'], 'producer-job-api.json',
                                                                  'producer-job.log', 'install-api.json')])
    arm = output['targets']['macos-arm64']
    ci = BASE / 'ci/source-ee0dabca/macos-arm64'
    platform_path = ci / 'platform-qualification.json'
    require(ident(platform_path)['sha256'] == 'f5050d2a2798ed2e4fde9686da8d40bc9ced669811fd3ebeddb1eed1e72b358b',
            'A48 platform handoff pin differs')
    qualification = read(platform_path)
    require(qualification['source_commit'] == SOURCE and qualification['source_tree'] == TREE
            and qualification['run_id'] == RUN and qualification['run_attempt'] == ATTEMPT
            and qualification['target'] == 'macos-arm64' and qualification['platform_qualified'] is True
            and qualification['release_qualified'] is False, 'External ARM qualification context differs')
    arm['external_platform_qualification'] = dict(owner='A48', **ident(platform_path), accepted=True,
                                                scope='External evidence; A57 executed no native test or validator.')
    a48docs = read(BASE / 'verification/a48/macos-arm64/docs-and-notices.json')
    own_docs = read(OWN / 'macos-arm64/docs-and-notices.json')
    compared_fields = ['required_metadata', 'source_comparisons', 'source_files', 'juce_license', 'source_time_guides']
    require(all(own_docs[key] == a48docs[key] for key in compared_fields), 'Independent ARM documentation differs from A48')
    arm['a48_document_content_equal_fields'] = compared_fields
    public_path = BASE / 'ci/source-ee0dabca/public-downloads/macos-arm64-full-get-0001.json'
    public = read(public_path)
    require(public['source'] == SOURCE and public['run_id'] == RUN and public['attempt_context'] == ATTEMPT
            and public['artifact_id'] == arm['artifact_id'] and public['artifact_name'] == arm['artifact_name']
            and public['expected_bytes'] == public['actual_bytes'] == arm['outer_zip']['bytes']
            and public['expected_sha256'] == public['actual_sha256'] == arm['outer_zip']['sha256']
            and public['method'] == 'GET' and public['credentials_supplied'] is False
            and public['status'] == 200 and public['public_route_verified'] is True
            and public['artifact_api_sha256'] == ident(ci / 'install-api.json')['sha256']
            and public['expires_at'] == arm['api_expires_at']
            and instant(public['completed_at']) < instant(public['expires_at'])
            and 'content-range' not in {k.lower() for k in public['headers']}, 'A51 full GET does not bind actual ARM package')
    arm['public_full_get_reconciled'] = True
    arm['public_evidence'] = dict(owner='A51', receipt=ident(public_path), url=public['requested_url'],
                                  method=public['method'], status=public['status'],
                                  started_at=public['started_at'], completed_at=public['completed_at'],
                                  full_bytes=public['actual_bytes'], sha256=public['actual_sha256'],
                                  independently_retrieved_by_a57=False)
    guides = {}
    for target in ACTUAL:
        docs = read(OWN / target / 'docs-and-notices.json')
        values = docs.get('guide_comparisons', docs.get('source_comparisons', []))
        guides[target] = {x['packaged']: x['sha256'] for x in values if 'inline_link_targets' in x}
    require(guides['macos-arm64'] == guides['windows-x86_64'] and len(guides['macos-arm64']) == 3,
            'Source-rewritten guide bytes differ across checked packages')
    output['two_target_guide_hashes'] = guides['macos-arm64']
    output['required_limits'] = [
        'A41 archive pass leaves native validation null and release qualification false; original package upload binding is separate.',
        'Current workflow requires 15 validator lanes (Linux 3, Windows 2, ARM Mac 5, Intel Mac 5); frozen A41 prose still says 13.',
        'Factory LOAD/RELOAD applies current factory sound controls and PANEL sources, preserving physical setup and separate ModernLayerState; it is not a complete session reset.',
        'Source-time guides intentionally preserve pending qualification and historical .4 links; final online publication is a separate action.',
        'Mac ad-hoc registration signing is not publisher signing or notarization; directory comparisons establish ordinary file bytes, not ACL/xattr/ownership/signature validity.',
        'Mac VST3 original source-path sequence is not an independent VST3 tree hash.',
        'Windows notice exception accepts exactly seven notices in one common raw or complete LF-to-CRLF mode; no mixed/partial or other-file normalization.',
        'JUCE license presence/inventory is verified; independent dependency-license source bytes are not claimed here.',
        'No product execution, real installation, interactive DAW compatibility, or listening result is inferred from archive headers.',
    ]
    output['reviewed_contracts'] = [ident(BASE / name) for name in ('reviews/a30.md', 'reviews/a42.md', 'reviews/a43.md',
                                                                 'reviews/a48.md', 'reviews/a51.md')]
    output['verifier'] = ident(Path(__file__))
    output_path = OWN / 'installation-matrix-phase1.json'
    with output_path.open('x') as stream:
        json.dump(output, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(status=output['status'], packages=2, public_downloads=1, out=str(output_path))))


if __name__ == '__main__':
    main()
