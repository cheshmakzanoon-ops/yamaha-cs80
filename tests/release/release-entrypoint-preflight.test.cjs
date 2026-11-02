// RED-first: both manual and v* tag release.yml events must independently
// enforce exact-main, four source-qualified workflow successes, and receipt.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const preflight = require('../../scripts/preflight-release-entrypoint.cjs');

const M = 'a'.repeat(40), S = 'b'.repeat(40), OTHER = 'c'.repeat(40);
const TAG_OBJECT = 'd'.repeat(40);
const NUMERICAL = 'release-numerical-qualification.yml';
const required = ['build.yml', 'dsp-measurements.yml', 'screenshots.yml', NUMERICAL];
let checks = 0;

async function scenario(overrides = {}) {
  const request = { validated_sha: M, expected_main_sha: S, ...(overrides.request || {}) };
  const calls = [];
  const github = { rest: {
    repos: {
      getContent: async args => {
        calls.push(['getContent', args]);
        if (overrides.noRequest) throw Error('Not Found');
        return { data: { encoding: 'base64', content: Buffer.from(JSON.stringify(request)).toString('base64') } };
      },
      compareCommits: async () => ({ data: {
        status: overrides.diverged ? 'diverged' : 'ahead', merge_base_commit: { sha: M },
        files: [{ filename: overrides.sourceChangedAfterM ? 'src/core/Engine.cpp'
          : 'docs/screenshots/provenance.json' }],
      } }),
    },
    git: {
      getRef: async args => {
        calls.push(['getRef', args]);
        if (args.ref === 'heads/main') return { data: { object: { sha: S, type: 'commit' } } };
        if (args.ref === 'tags/v1.2.3') return { data: { object: {
          type: overrides.lightweightTag ? 'commit' : 'tag',
          sha: overrides.lightweightTag ? (overrides.badTag ? OTHER : S) : TAG_OBJECT,
        } } };
        throw Error('Unexpected tag ref ' + args.ref);
      },
      getTag: async args => {
        calls.push(['getTag', args]);
        return { data: { object: { type: 'commit', sha: overrides.badTag ? OTHER : S } } };
      },
    },
    actions: {
      listWorkflowRuns: async args => {
        calls.push(['listWorkflowRuns', args]);
        if (args.workflow_id === 'release.yml') return { data: { workflow_runs: [
          { id: 777, head_sha: S, head_branch: 'main', status: 'in_progress' },
          ...(overrides.duplicate ? [{ id: 776, head_sha: S, head_branch: 'main',
              status: 'completed', conclusion: 'success' }] : []),
        ] } };
        if (args.workflow_id === NUMERICAL && overrides.noNumerical)
          return { data: { workflow_runs: [] } };
        return { data: { workflow_runs: [{
          head_sha: args.workflow_id === NUMERICAL && overrides.wrongNumericalSource ? OTHER : M,
          head_branch: args.workflow_id === NUMERICAL && overrides.prNumerical ? 'feature' : 'main',
          status: args.workflow_id === NUMERICAL && overrides.pendingNumerical ? 'in_progress' : 'completed',
          conclusion: args.workflow_id === NUMERICAL && overrides.failedNumerical ? 'failure' : 'success',
          html_url: args.workflow_id,
        }] } };
      },
    },
  } };
  const eventName = overrides.eventName || 'workflow_dispatch';
  const context = { repo: { owner: 'review', repo: 'cs80' }, eventName, runId: 777,
    ref: overrides.ref || (eventName === 'push' ? 'refs/tags/v1.2.3' : 'refs/heads/main'),
    sha: overrides.eventSha || S };
  const checkoutSha = overrides.checkoutSha || S;
  const result = await preflight({ github, context, core: { info() {} }, checkoutSha });
  return { result, calls };
}

(async () => {
  const manual = await scenario();
  assert.equal(manual.result.authorized, true); checks++;
  assert.equal(manual.result.expected_main_sha, S); checks++;
  assert.ok(required.every(id => manual.calls.some(([action, arg]) =>
    action === 'listWorkflowRuns' && arg.workflow_id === id
    && arg.head_sha === M && arg.event === 'push'))); checks++;
  assert.ok(manual.calls.some(([action, arg]) => action === 'getContent'
    && arg.path === '.github/release-request.json'
    && arg.ref === 'codex/cs80-release-dispatch')); checks++;

  const tag = await scenario({ eventName: 'push' });
  assert.equal(tag.result.authorized, true); checks++;
  assert.ok(tag.calls.some(([action, arg]) => action === 'getTag' && arg.tag_sha === TAG_OBJECT)); checks++;
  const lightweight = await scenario({ eventName: 'push', lightweightTag: true });
  assert.equal(lightweight.result.authorized, true); checks++;

  const negatives = [
    ['manual on feature', { ref: 'refs/heads/feature' }],
    ['manual wrong event SHA', { eventSha: OTHER }],
    ['manual checkout wrong source', { checkoutSha: OTHER }],
    ['tag missing semver', { eventName: 'push', ref: 'refs/tags/vbad' }],
    ['branch push not tag', { eventName: 'push', ref: 'refs/heads/main' }],
    ['tag annotated points elsewhere', { eventName: 'push', badTag: true }],
    ['tag lightweight points elsewhere', { eventName: 'push', lightweightTag: true, badTag: true }],
    ['missing release request', { noRequest: true }],
    ['request wrong main', { request: { expected_main_sha: OTHER } }],
    ['request diverged', { diverged: true }],
    ['non-screenshot after M', { sourceChangedAfterM: true }],
    ['missing fourth numerical workflow', { noNumerical: true }],
    ['numerical failure', { failedNumerical: true }],
    ['numerical pending', { pendingNumerical: true }],
    ['numerical from PR', { prNumerical: true }],
    ['numerical stale SHA', { wrongNumericalSource: true }],
    ['already released', { duplicate: true }],
  ];
  for (const [label, overrides] of negatives) {
    await assert.rejects(() => scenario(overrides), undefined, label);
    checks++;
  }
  const workflow = fs.readFileSync(path.resolve(__dirname, '../../.github/workflows/release.yml'), 'utf8');
  assert.match(workflow, /^\s{2}preflight:\s*$/m); checks++;
  for (const job of ['macos-universal', 'windows-installer', 'linux-packages']) {
    const pos = workflow.indexOf(`  ${job}:`);
    assert.ok(pos > 0, `${job} preserved`); checks++;
    assert.match(workflow.slice(pos, pos + 270), /needs:\s*preflight/, `${job} depends on preflight`);
    checks++;
    assert.match(workflow.slice(pos, pos + 500), /ref:\s*\$\{\{\s*needs\.preflight\.outputs\.expected_main_sha\s*\}\}/,
      `${job} checkout pinned to preflight-approved SHA`); checks++;
  }
  console.log(`Manual/tag release entrypoint preflight: ${checks} checks, 0 failures`);
})().catch(error => { console.error(error); process.exitCode = 1; });
