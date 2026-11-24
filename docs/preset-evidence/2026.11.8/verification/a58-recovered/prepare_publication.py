#!/usr/bin/env python3
"""Recovered A58: prepare/verify publication, never write repository files.

All actual acceptance belongs to the separately verified aggregate. This helper
only cross-checks supplied evidence and publication consistency. It makes no
network requests and leaves its own release_qualified flag false.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
from zoneinfo import ZoneInfo

REPO = 'cheshmakzanoon-ops/yamaha-cs80'
REPO_ID = 1436742091
SOUND_SOURCE = 'ef1c472b161c37c490945f7c469086508aae6aef'
OLD_RUN = 45439302351
REVISION = '2026.11.8'
HISTORY_SHA = '8b9bfd30b8ccf57e98a4e4d616a6b93767d3d064df5c68cdaac79b5b954bf17e'
OLD_HISTORY = '## Current published packages — 2026.11.4 instrument voicing'
NEW_HISTORY = '## Historical qualified packages — 2026.11.4 instrument voicing'
CI_HEADING = '## Current qualified packages — 2026.11.8 keyboard response and preset stability'
TARGETS = {
    'windows-x86_64': ('cs80-windows-install-bundle', 'Windows x64', 'VST3, Standalone', 261),
    'macos-arm64': ('cs80-macos-15-install-bundle', 'macOS Apple Silicon (arm64)', 'VST3, AU, Standalone', 283),
    'macos-x86_64': ('cs80-macos-15-intel-install-bundle', 'macOS Intel (x86-64)', 'VST3, AU, Standalone', 283),
    'linux-x86_64': ('cs80-linux-install-bundle', 'Linux x86-64 (Ubuntu 24.04 build)', 'VST3, Standalone', 619),
    'audition': (None, 'Audition', '', None),
}
AUDITION = dict(wav_count=256, selected_program_count=95, folders=60, regular_files=321,
                checksums=320, frames=71385600, seconds=1487.2,
                inventory_programs=523, inventory_parameters=204)
COUNT_KEYS = ('registered', 'passed', 'failed', 'formal_skipped', 'disabled', 'unknown')


def need(ok, message):
    if not ok:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def pairs(rows):
    result = {}
    for key, value in rows:
        need(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def read_json(path):
    raw = Path(path).read_bytes()
    need(len(raw) <= 4 * 1024**2, 'Metadata exceeds 4 MiB')
    return json.loads(raw, object_pairs_hook=pairs), {'bytes': len(raw), 'sha256': digest(raw)}


def section(text, heading):
    marker = heading + '\n'
    need(text.count(marker) == 1, 'Missing or duplicate heading: ' + heading)
    start = text.index(marker)
    end = text.find('\n## ', start + len(marker))
    return text[start:] if end < 0 else text[start:end + 1]


def history(text):
    headings = [h for h in (OLD_HISTORY, NEW_HISTORY) if h + '\n' in text]
    need(len(headings) == 1, 'Exactly one .4 history section required')
    body = section(text, headings[0]).split('\n', 1)[1]
    need(digest(body.encode()) == HISTORY_SHA, 'Historical .4 body changed')


def context(value, binding):
    need(all(value.get(k) == binding[k] for k in ('source_commit', 'source_tree', 'run_id', 'run_attempt')),
         'Qualified aggregate and explicit expected binding differ')


def validate(args):
    binding, binding_ref = read_json(args.binding)
    source, run_id, attempt = binding.get('source_commit'), binding.get('run_id'), binding.get('run_attempt')
    need(isinstance(source, str) and re.fullmatch('[0-9a-f]{40}', source)
         and source != SOUND_SOURCE, 'Explicit NEW source binding required; ef1c is superseded')
    need(isinstance(binding.get('source_tree'), str) and re.fullmatch('[0-9a-f]{40}', binding['source_tree']),
         'Complete expected source tree required')
    need(type(run_id) is int and run_id > 0 and run_id != OLD_RUN
         and type(attempt) is int and attempt > 0, 'Explicit NEW native run/attempt required')
    need(binding.get('reviewed_sound_source') == SOUND_SOURCE
         and binding.get('sound_source_unchanged') is True,
         'Reviewed unchanged sound-source comparison required for these publication claims')
    checker_sha = binding.get('public_checker_sha256')
    need(isinstance(checker_sha, str) and re.fullmatch('[0-9a-f]{64}', checker_sha),
         'Pin the actual final-source public checker SHA256')
    qualified, qualified_ref = read_json(args.qualification)
    need(qualified.get('release_qualified') is True, 'Separate completed release qualification required')
    context(qualified, binding)
    run, run_ref = read_json(args.run_api)
    need(run.get('id') == run_id and run.get('head_sha') == source
         and run.get('run_attempt') == attempt and run.get('status') == 'completed'
         and run.get('conclusion') == 'success' and run.get('workflow_id') == 399616500,
         'Actual complete native build run/attempt/source does not match')
    need(run.get('repository', {}).get('id') == run.get('head_repository', {}).get('id') == REPO_ID,
         'Build repository identity differs')
    collection, collection_ref = read_json(args.collection)
    artifacts = collection.get('artifacts')
    need(isinstance(artifacts, list) and collection.get('total_count') == len(artifacts),
         'Complete artifact collection required')
    ids = qualified.get('advertised_artifact_ids')
    need(isinstance(ids, dict) and set(ids) == set(TARGETS), 'Exactly five approved target IDs required')
    advertised = {}
    for target, (name, label, formats, registered) in TARGETS.items():
        name = name or ('cs80-curated-preset-audition-' + source)
        rows = [row for row in artifacts if row.get('name') == name]
        need(len(rows) == 1, 'Missing/ambiguous artifact: ' + name)
        api = rows[0]
        need(type(ids[target]) is int and ids[target] > 0 and api.get('id') == ids[target],
             'Approved/API artifact ID differs: ' + target)
        wr = api.get('workflow_run', {})
        need(wr.get('id') == run_id and wr.get('head_sha') == source
             and wr.get('repository_id') == wr.get('head_repository_id') == REPO_ID,
             'Artifact source/run/repository differs: ' + target)
        need('run_attempt' not in wr or wr['run_attempt'] == attempt, 'Declared artifact attempt differs')
        route = f'https://api.github.com/repos/{REPO}/actions/artifacts/{api["id"]}'
        need(api.get('url') == route and api.get('archive_download_url') == route + '/zip', 'API route differs')
        expiry = datetime.fromisoformat(api['expires_at'].replace('Z', '+00:00'))
        need(api.get('expired') is False and expiry.tzinfo is not None
             and expiry > datetime.now(timezone.utc), 'Artifact expired')
        need(type(api.get('size_in_bytes')) is int and 0 < api['size_in_bytes'] <= 1024**3,
             'Invalid API byte count')
        need(isinstance(api.get('digest'), str) and re.fullmatch('sha256:[0-9a-f]{64}', api['digest']),
             'API SHA256 missing')
        receipt, receipt_ref = read_json(args.public_directory / (target + '-public-receipt.json'))
        # Accept only the two reviewed A46 receipt schemas; neither is an aggregate qualification.
        if 'expected_context' in receipt:
            c = receipt['expected_context']
            need(c == dict(source=source, run_id=run_id, attempt=attempt, artifact_id=api['id'],
                           name=name, repository=REPO, repository_id=REPO_ID), 'Original receipt context differs')
            need(receipt.get('checker_sha256') == checker_sha, 'Public checker differs')
            pg = receipt.get('public_get', {})
            full = (pg.get('method') == 'GET' and pg.get('http_status') == 200
                    and pg.get('request_authorization') is False and pg.get('request_range') is False
                    and pg.get('complete_body_read_to_eof') is True)
        else:
            need((receipt.get('source'), receipt.get('source_tree'), receipt.get('run_id'), receipt.get('run_attempt'))
                 == (source, binding['source_tree'], run_id, attempt), 'Recovered receipt context differs')
            need(receipt.get('checker', {}).get('sha256') == checker_sha, 'Recovered public checker differs')
            pg = receipt.get('public_get', {})
            full = (pg.get('status') == 200 and pg.get('authorization') is False
                    and pg.get('range') is False and pg.get('full_body_to_eof') is True)
        need(full, 'Complete unauthenticated non-range GET required: ' + target)
        url = f'https://nightly.link/{REPO}/actions/artifacts/{api["id"]}.zip'
        need(pg.get('url') == url and pg.get('bytes') == api['size_in_bytes']
             and 'sha256:' + str(pg.get('sha256')) == api['digest'], 'Public/API URL or complete bytes differ')
        need(all(receipt.get('artifact', {}).get(k) == api.get(k)
                 for k in ('id', 'name', 'size_in_bytes', 'digest', 'workflow_run')),
             'Public receipt artifact metadata differs')
        advertised[target] = dict(artifact_id=api['id'], label=label, formats=formats, name=name,
                                   url=url, bytes=api['size_in_bytes'], sha256=api['digest'][7:],
                                   expires_at=api['expires_at'], public_receipt=receipt_ref)
    need(len(set(ids.values())) == 5, 'Duplicate advertised IDs')
    native = qualified.get('native', {})
    sums = {k: 0 for k in COUNT_KEYS}
    for target, (_, _, _, expected) in TARGETS.items():
        if expected is None:
            continue
        counts = native.get('by_target', {}).get(target, {}).get('observed_counts', {})
        need(all(type(counts.get(k)) is int and counts[k] >= 0 for k in COUNT_KEYS), 'Observed native counts missing')
        need(counts['registered'] == expected and counts['failed'] == counts['disabled'] == counts['unknown'] == 0
             and counts['passed'] + counts['formal_skipped'] == expected, 'Native accounting differs')
        for key in COUNT_KEYS:
            sums[key] += counts[key]
        advertised[target]['native'] = counts
    totals = native.get('totals', {})
    need(all(type(totals.get(k)) is int and totals[k] == v for k, v in sums.items()), 'Native totals disagree')
    nested = totals.get('nested_python_skipped')
    need(nested is None or (type(nested) is int and nested >= 0), 'Nested skips must be observed integer or unknown')
    pluginval = qualified.get('pluginval', {}).get('summary', {})
    need(pluginval.get('required_lanes') == pluginval.get('passed_lanes') == 15, 'All 15 validator lanes required')
    need(all(type(pluginval.get(k)) is int and pluginval[k] >= 0
             for k in ('validation_invocations', 'runtime_validation_retries'))
         and pluginval['validation_invocations'] >= 15, 'Actual validator invocation/retry accounting missing')
    audition = qualified.get('audition', {}).get('observed', {})
    need(all(audition.get(k) == v for k, v in AUDITION.items()), 'Actual audition dimensions differ')
    expiry = min(datetime.fromisoformat(row['expires_at'].replace('Z', '+00:00')) for row in advertised.values())
    return dict(source_commit=source, source_tree=binding['source_tree'], run_id=run_id, run_attempt=attempt,
                advertised=advertised, native_totals=totals, pluginval=pluginval, audition=audition,
                expiry_display=expiry.astimezone(ZoneInfo('America/Halifax')).strftime('%d %B %Y at %H:%M:%S %Z')
                + ' (' + expiry.astimezone(timezone.utc).strftime('%H:%M:%S UTC') + ')',
                inputs=dict(binding=binding_ref, qualification=qualified_ref, run_api=run_ref, collection=collection_ref),
                release_qualified=False, scope='Publication consistency only; aggregate qualification is separate.')


def download_text(facts, heading, evidence_link, prefix):
    source, run_id, attempt = (facts[k] for k in ('source_commit', 'run_id', 'run_attempt'))
    n, v = facts['native_totals'], facts['pluginval']
    lines = [heading, '', '**2026.11.8 — response and stability improvements to existing programs.**',
             'All **523 names, categories and addresses** remain intact, including the two Init programs.',
             'No presets were added. All 521 musical programs retain the cumulative response-control',
             'programming relative to the earlier published 2026.11.4 bank.', '',
             '| Platform | Included formats | Native passed / registered | Verified installation download |',
             '| --- | --- | ---: | --- |']
    for target, row in facts['advertised'].items():
        if target != 'audition':
            c = row['native']
            lines.append(f'| {row["label"]} | {row["formats"]} | {c["passed"]} / {c["registered"]} | [Download ZIP]({row["url"]}) |')
    nested = n.get('nested_python_skipped')
    nested_text = (f'{nested} nested Python skips are recorded separately.' if type(nested) is int
                   else 'Nested Python skips remain separate; their aggregate count is not established.')
    lines += ['', f'The four native suites recorded **{n["passed"]:,} passed / {n["registered"]:,} registered,',
              f'zero failed and {n["formal_skipped"]} formally skipped**. {nested_text}',
              f'All **15 pluginval 1.0.4 lanes at strictness 10** passed; {v["validation_invocations"]}',
              f'validation invocations and {v["runtime_validation_retries"]} runtime validation retries are recorded.',
              'Original exits, reviewed skips, any retries and source-bound package/payload checks are retained in',
              f'[the actual release qualification]({evidence_link}). Supporting sanitizer executions are separate',
              'from these native counts; three supporting shards cover the 232-test core roster.', '',
              f'The verified [audition ZIP]({facts["advertised"]["audition"]["url"]}) contains **256 unnormalized',
              '48 kHz stereo float WAVs from 95 existing programs**, totaling **24 minutes 47.2 seconds**,',
              'with the full **523 × 204** inventory. The actual 60 folders, 321 files, 320 checksums,',
              'settings, metrics and samples passed inspection. Every sample is finite and below full scale.',
              'These checks do not establish human listening approval or exact acoustic replication.', '',
              'Unpack the outer ZIP, verify `SHA256SUMS`, then unpack the inner archive with plugin/app bundles',
              'intact. Linux provides VST3 and Standalone in a tar archive. Choose the Mac build for your',
              '**host process architecture**. These are CI installation bundles without publisher release',
              'signing or Apple notarization; Linux requires compatible system libraries.',
              f'The earliest advertised expiry is **{facts["expiry_display"]}**. These Actions artifacts are',
              'retained downloads, not permanent release assets. All five exact nightly.link routes passed',
              'complete unauthenticated GETs with API-matching bytes and SHA-256. The',
              f'[original run’s Artifacts list](https://github.com/{REPO}/actions/runs/{run_id}#artifacts)',
              'is the fallback while retained.', '',
              '**Release all keys and the sustain pedal, use Library → FACTORY LIBRARY → LOAD / RELOAD,',
              'then play fresh notes.** Saved sessions retain their stored vectors; the shared sound engine',
              'has changed. Exact earlier audio needs the earlier binary or an audio render.', '',
              f'Binary source is [`{source[:7]}`](https://github.com/{REPO}/commit/{source}),',
              f'from [build {run_id}, attempt {attempt}](https://github.com/{REPO}/actions/runs/{run_id}).',
              'Later documentation commits do not change the packaged bytes. Bundled `SOURCE.txt` and',
              'manifests identify the full source; guides retain their source-time wording.',
              f'[Revision report]({prefix}CIRCUIT_LIBRARY_2026_11_8.md) ·',
              f'[Sixty-worker recovery record]({prefix}TEAM_REVIEW_2026_11_8.md) ·',
              f'[Installation guide]({prefix}INSTALL_AND_QUICKSTART.md#install-only-the-formats-you-need)', '']
    return '\n'.join(lines)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode', choices=('prepare', 'verify'))
    for name in ('binding', 'qualification', 'run-api', 'collection', 'public-directory', 'documents', 'out'):
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--evidence-link', required=True, help='README-relative actual qualification JSON path')
    args = p.parse_args()
    need(not args.out.exists(), 'Use a fresh output directory')
    link = Path(args.evidence_link)
    need(not link.is_absolute() and '..' not in link.parts
         and args.evidence_link.startswith('docs/preset-evidence/2026.11.8/')
         and not any(c.isspace() for c in args.evidence_link), 'Use a safe relative .8 qualification link')
    facts = validate(args)
    readme = (args.documents / 'README.md').read_text()
    status = (args.documents / 'docs/CI_DISTRIBUTION_STATUS.md').read_text()
    revision = (args.documents / 'docs/CIRCUIT_LIBRARY_2026_11_8.md').read_text()
    team = (args.documents / 'docs/TEAM_REVIEW_2026_11_8.md').read_text()
    history(status)
    need('**Evidence status:**' in revision and '**historical session observations**' in revision,
         'Preserve A55 recovery/evidence distinctions')
    need('**This is a reconstructed coordination summary' in team
         and 'not a claim that all\n60 original report files have now been recovered' in team,
         'Preserve actual sixty-worker recovery distinction')
    docs_link = args.evidence_link[len('docs/'):]
    readme_fragment = download_text(facts, '## Downloads', args.evidence_link, 'docs/')
    ci_fragment = download_text(facts, CI_HEADING, docs_link, '')
    release_fragment = '\n'.join(['## Release qualification', '',
        f'Binary source `{facts["source_commit"]}`, tree `{facts["source_tree"]}`, revision **2026.11.8**,',
        f'was accepted from native build **{facts["run_id"]}, attempt {facts["run_attempt"]}**.',
        'The four fresh native targets, all 15 validator lanes, actual installation and audition archives,',
        f'and five complete public GETs are bound in [the release qualification]({docs_link}).',
        'The sound implementation remains the reviewed ef1c snapshot with subsequent test/workflow',
        'portability corrections. Historical local measurements and the lost-evidence disclosure above',
        'retain their stated limits; new native evidence does not recreate the missing raw probes.', ''])
    status_fragment = section(revision, '## Status')
    if args.mode == 'prepare':
        old = ('The published downloads still identify the qualified 2026.11.4 source until the\n'
               'new native builds, validators and installation archives complete qualification.')
        need(status_fragment.count(old) == 1, 'Review the changed revision Status before preparing an overlay')
        new = (f'Qualified binary source is `{facts["source_commit"]}`, from native build\n'
               f'**{facts["run_id"]}, attempt {facts["run_attempt"]}**. The\n'
               f'[actual release qualification]({docs_link}) binds the accepted archives and downloads.')
        status_fragment = status_fragment.replace(old, new)
    team_fragment = '\n'.join(['## Current qualified binary source', '',
        f'The accepted binary source is `{facts["source_commit"]}`, tree `{facts["source_tree"]}`,',
        f'from native build **{facts["run_id"]}, attempt {facts["run_attempt"]}**.',
        f'[Actual source-bound release qualification]({docs_link}) records all four fresh native targets,',
        '15 validator lanes, the installation and audition archives, and five complete public GETs.',
        'Supporting sanitizer results remain separate from the 1,446 native registrations.',
        'The reviewed sound implementation remains ef1c472; test/workflow portability corrections',
        'do not change that programming. The original report/data loss and reconstruction disclosures',
        'below remain applicable; this fresh qualification does not recover missing historical measurements.', ''])
    if args.mode == 'verify':
        # Exact generated blocks make counts, links and expiry reviewable together.
        need(section(readme, '## Downloads').strip() == readme_fragment.strip(), 'README publication block differs')
        need(section(status, CI_HEADING).strip() == ci_fragment.strip(), 'CI publication block differs')
        need(NEW_HISTORY + '\n' in status, 'Label the old .4 qualification historical')
        need(section(revision, '## Release qualification').strip() == release_fragment.strip(), 'Revision release block differs')
        need(facts['source_commit'] in status_fragment
             and 'published downloads still identify the qualified 2026.11.4' not in status_fragment,
             'Revision Status retains the old download claim or lacks the new source')
        need('## Current sound development — 2026.11.8' not in readme, 'Rename the current README revision section')
        current = section(readme, '## Current sound revision — 2026.11.8')
        need(not re.search(r'are continuing|will move|qualification remains pending', current, re.I), 'Stale active README status')
        need(section(team, '## Current qualified binary source').strip() == team_fragment.strip(), 'Team current source/run differs')
        need('The product source remains [ef1c472]' not in team, 'Clarify ef1c as the reviewed sound source in team history')
        need(digest((args.documents / link).read_bytes()) == facts['inputs']['qualification']['sha256'],
             'Published aggregate bytes differ')
        facts['document_consistency_verified'] = True
    args.out.mkdir(parents=True)
    for name, content in [('README-downloads.md', readme_fragment), ('CI-current-qualified.md', ci_fragment),
                          ('REVISION-status.md', status_fragment),
                          ('REVISION-release-qualification.md', release_fragment),
                          ('TEAM-current-qualified.md', team_fragment)]:
        (args.out / name).write_text(content)
    (args.out / 'publication-facts.json').write_text(json.dumps(facts, indent=2) + '\n')
    print(json.dumps({'mode': args.mode, 'prepared_targets': 5, 'release_qualified': False, 'out': str(args.out)}))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(type(error).__name__ + ': ' + str(error), file=sys.stderr)
        sys.exit(2)
