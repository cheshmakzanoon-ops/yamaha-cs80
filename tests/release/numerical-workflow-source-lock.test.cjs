'use strict';

// Preregistered structural controls for a fourth, independent exact-main
// numerical producer. Source-checked checkout must own the raw evidence;
// supplied receipt strings or prior artifacts are not qualifications.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '../..');
const workflow = () => fs.readFileSync(path.join(root, '.github/workflows/release-numerical-qualification.yml'), 'utf8');

function requireMainOnly(source) {
  assert.match(source, /on:\s*\n\s+push:\s*\n\s+branches:\s*\n\s+- main(?:\s|$)/);
  assert.doesNotMatch(source, /workflow_dispatch\s*:/);
  assert.doesNotMatch(source, /pull_request\s*:/);
  assert.doesNotMatch(source, /tags\s*:/);
  assert.match(source, /permissions:\s*\n\s+contents: read/);
  assert.doesNotMatch(source, /contents: write|actions: write|id-token: write/);
  assert.match(source, /persist-credentials: false/);
  assert.match(source, /fetch-depth: 0/);
  const jobs = source.split(/^jobs:\s*$/m)[1];
  assert.ok(jobs, 'the independent job is required');
  const jobNames = [...jobs.matchAll(/^  ([a-z][a-z0-9-]*):\s*$/gm)].map(match => match[1]);
  assert.deepEqual(jobNames, ['trusted-source-and-same-host-evidence']);
}

function requireSameJobRawCapture(source) {
  assert.match(source, /python scripts\/full_engine_audit_gate\.py --self-test/);
  assert.match(source, /python scripts\/full_engine_audit_gate\.py --preflight/);
  assert.match(source, /cs80_engine_audit --out artifacts\/audit --suite all --rates 44100,48000,96000/);
  assert.match(source, /python scripts\/full_engine_audit_gate\.py --finalize/);
  assert.match(source, /python scripts\/run_release_cpu_producer\.py/);
  assert.match(source, /python scripts\/release_numerical_gate\.py/);
  assert.match(source, /--before-round\s+"\$CPU_OUT\/before\/round-1"/);
  assert.match(source, /--before-round\s+"\$CPU_OUT\/before\/round-2"/);
  assert.match(source, /--before-round\s+"\$CPU_OUT\/before\/round-3"/);
  assert.match(source, /--after-round\s+"\$CPU_OUT\/after\/round-1"/);
  assert.match(source, /--after-round\s+"\$CPU_OUT\/after\/round-2"/);
  assert.match(source, /--after-round\s+"\$CPU_OUT\/after\/round-3"/);
  assert.doesNotMatch(source, /download-artifact|workflow_run|curl\s.*qualification-receipt/);
  const stages = [
    'scripts/prepare_release_numerical_request.py',
    'cs80_engine_audit --out artifacts/audit --suite all --rates 44100,48000,96000',
    'scripts/full_engine_audit_gate.py --finalize',
    'scripts/run_release_cpu_producer.py',
    'scripts/release_numerical_gate.py',
  ].map(stage => source.indexOf(stage));
  assert.ok(stages.every(position => position >= 0));
  assert.deepEqual(stages, [...stages].sort((a, b) => a - b));
}

function requireTrustedBindings(source) {
  assert.match(source, /scripts\/prepare_release_numerical_request\.py/);
  assert.match(source, /--expected-main-sha\s+"\$GITHUB_SHA"/);
  assert.match(source, /--expected-source-tree-sha\s+"\$EXPECTED_SRC_TREE"/);
  assert.match(source, /--expected-runner-job-id\s+"\$GITHUB_RUN_ID\/\$GITHUB_RUN_ATTEMPT\/\$GITHUB_JOB"/);
  assert.match(source, /\$GITHUB_EVENT_NAME/);
  assert.match(source, /\$GITHUB_REF/);
  assert.match(source, /refs\/remotes\/origin\/main/);
  assert.match(source, /--audit-dir artifacts\/audit/);
}

test('independent numerical workflow accepts only a push to exact main', () => {
  requireMainOnly(workflow());
});

test('single job directly builds original audit and collects the six raw CPU rounds', () => {
  requireSameJobRawCapture(workflow());
});

test('trusted source identity, current-main lease, and same-job receipt bind gate input', () => {
  requireTrustedBindings(workflow());
});

test('malicious event, source, and evidence-producer mutations fail the workflow contract', () => {
  const original = workflow();
  const mutants = [
    ['tag trigger', value => value.replace('    branches:\n      - main', '    tags:\n      - v*'), requireMainOnly],
    ['manual trigger', value => value.replace('on:\n  push:', 'on:\n  workflow_dispatch:\n  push:'), requireMainOnly],
    ['broad permissions', value => value.replace('contents: read', 'contents: write'), requireMainOnly],
    ['secondary untrusted job', value => value.replace('jobs:\n', 'jobs:\n  side-channel:\n'), requireMainOnly],
    ['missing CPU runner', value => value.replace('python scripts/run_release_cpu_producer.py', 'python scripts/unknown_producer.py'), requireSameJobRawCapture],
    ['missing raw round', value => value.replace('--after-round "$CPU_OUT/after/round-3"', '--after-round "$CPU_OUT/after/round-2"'), requireSameJobRawCapture],
    ['self-reported SHA', value => value.replace('--expected-main-sha "$GITHUB_SHA"', '--expected-main-sha "$SUPPLIED_SHA"'), requireTrustedBindings],
    ['external evidence', value => value.replace('python scripts/run_release_cpu_producer.py', 'curl qualification-receipt && python scripts/run_release_cpu_producer.py'), requireSameJobRawCapture],
  ];
  for (const [label, mutate, guard] of mutants) {
    assert.throws(() => guard(mutate(original)), undefined, label);
  }
});
