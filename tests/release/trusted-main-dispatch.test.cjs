// RED-first: a request branch is data only. A maintainer starts this from
// current main with an exact request-commit input; no branch push may run
// privileged, request-supplied YAML or JavaScript.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const dispatch = require('../../scripts/dispatch-reviewed-main-request.cjs');

const OWNER = 'cheshmakzanoon-ops', REPO = 'yamaha-cs80';
const M = 'a'.repeat(40), S = 'b'.repeat(40), R = 'c'.repeat(40);
const H = 'd'.repeat(40), OTHER = 'e'.repeat(40);
const REQUEST_BRANCH = 'heads/codex/cs80-release-dispatch';
const REQUEST_FILE = '.github/release-request.json';
const NUMERICAL = 'release-numerical-qualification.yml';

async function scenario(o = {}) {
  const calls = [];
  const request = { requested_by: OWNER, pr_number: 6,
    reviewed_pr_head_sha: H, pr_merge_commit_sha: M,
    expected_main_sha: S, validated_sha: M, ...(o.request || {}) };
  const github = { rest: {
    git: { getRef: async args => {
      calls.push(['getRef', args]);
      if (args.ref === 'heads/main') return { data: { object: {
        type: 'commit', sha: o.mainMoved ? OTHER : S } } };
      if (args.ref === REQUEST_BRANCH) {
        if (o.requestBranchMissing) throw Error('Not Found');
        return { data: { object: {
          type: o.requestBranchTag ? 'tag' : 'commit',
          sha: o.requestBranchMoved ? OTHER : R } } };
      }
      throw Error('Unexpected git ref ' + args.ref);
    } },
    repos: {
      getContent: async args => {
        calls.push(['getContent', args]);
        if (o.requestMissing) throw Error('Not Found');
        return { data: { type: 'file', encoding: o.contentEncoding || 'base64',
          content: Buffer.from(o.malformedJson ? 'this-is-not-json' :
            JSON.stringify(request)).toString('base64') } };
      },
      compareCommits: async args => {
        calls.push(['compareCommits', args]);
        if (args.base === S && args.head === R) {
          const file = o.branchScriptModified
            ? 'scripts/dispatch-reviewed-release.cjs'
            : o.branchWorkflowModified
              ? '.github/workflows/dispatch-reviewed-release.yml'
              : REQUEST_FILE;
          return { data: { status: o.requestDiverged ? 'diverged' : 'ahead',
            merge_base_commit: { sha: o.requestWrongMergeBase ? OTHER : S },
            files: o.extraRequestFiles ? [
              { filename: REQUEST_FILE, status: 'added' },
              { filename: 'scripts/release_numerical_gate.py', status: 'modified' },
            ] : o.noRequestDiff ? [] : [{ filename: file,
              status: o.requestRenamed ? 'renamed' : 'added',
              ...(o.requestRenamed ? { previous_filename: 'src/core/Engine.cpp' } : {}) }] } };
        }
        if (args.base === M && args.head === S)
          return { data: { status: 'ahead', merge_base_commit: { sha: M },
            files: [{ filename: 'docs/screenshots/final.png', status: 'modified' }] } };
        throw Error('Unexpected comparison ' + JSON.stringify(args));
      },
    },
    pulls: { get: async args => {
      calls.push(['pulls.get', args]);
      return { data: { number: 6, state: o.prOpen ? 'open' : 'closed',
        merged: !o.prUnmerged, draft: !!o.prDraft,
        base: { ref: o.prWrongBase ? 'feature' : 'main' },
        head: { sha: o.prWrongHead ? OTHER : H },
        merge_commit_sha: o.prWrongMerge ? OTHER : M } };
    } },
    actions: {
      listWorkflowRuns: async args => {
        calls.push(['listWorkflowRuns', args]);
        if (args.workflow_id === 'release.yml') return { data: { workflow_runs: [] } };
        return { data: { workflow_runs: o.numericalMissing && args.workflow_id === NUMERICAL
          ? [] : [{ head_sha: o.wrongRunSha ? OTHER : M, head_branch: 'main',
            status: 'completed', conclusion: 'success', html_url: args.workflow_id }] } };
      },
      createWorkflowDispatch: async args => { calls.push(['createWorkflowDispatch', args]); },
    },
  } };
  const context = { repo: { owner: o.wrongRepo ? 'other' : OWNER, repo: REPO },
    actor: o.wrongActor ? 'unreviewed-collaborator' : OWNER,
    eventName: o.eventName || 'workflow_dispatch',
    ref: o.ref || 'refs/heads/main', sha: o.eventSha || S };
  const result = await dispatch({ github, context, core: { info() {} },
    requestSha: o.requestSha || R, checkoutSha: o.checkoutSha || S,
    approvedMainSha: o.approvedMainSha || S });
  return { calls, result };
}

(async () => {
  let checks = 0;
  const good = await scenario();
  assert.equal(good.result.dispatched, true); checks++;
  assert.deepEqual(good.calls.filter(([verb]) => verb === 'createWorkflowDispatch'), [
    ['createWorkflowDispatch', { owner: OWNER, repo: REPO, workflow_id: 'release.yml', ref: 'main' }],
  ]); checks++;
  assert.ok(good.calls.some(([verb, args]) => verb === 'getContent'
    && args.ref === R && args.path === REQUEST_FILE)); checks++;
  assert.ok(good.calls.some(([verb, args]) => verb === 'pulls.get'
    && args.pull_number === 6)); checks++;
  assert.ok(good.calls.every(([verb, args]) => verb !== 'getContent'
    || args.path === REQUEST_FILE)); checks++;

  const negatives = [
    ['push event disallowed', { eventName: 'push' }],
    ['non-main manual ref', { ref: 'refs/heads/codex/cs80-release-dispatch' }],
    ['unreviewed caller', { wrongActor: true }],
    ['wrong repository', { wrongRepo: true }],
    ['stale main event SHA', { eventSha: OTHER }],
    ['stale checked-out main', { checkoutSha: OTHER }],
    ['stale API approved main', { approvedMainSha: OTHER }],
    ['main moved', { mainMoved: true }],
    ['short request SHA', { requestSha: 'abc' }],
    ['request branch missing', { requestBranchMissing: true }],
    ['request branch moved', { requestBranchMoved: true }],
    ['request branch tag object', { requestBranchTag: true }],
    ['request file absent', { requestMissing: true }],
    ['request not base64', { contentEncoding: 'utf-8' }],
    ['request malformed JSON', { malformedJson: true }],
    ['request not based on exact main', { requestDiverged: true }],
    ['request wrong merge base', { requestWrongMergeBase: true }],
    ['request diff empty', { noRequestDiff: true }],
    ['altered script on request branch', { branchScriptModified: true }],
    ['altered workflow YAML on request branch', { branchWorkflowModified: true }],
    ['other code alongside request JSON', { extraRequestFiles: true }],
    ['request file renamed from source', { requestRenamed: true }],
    ['request actor spoofed in JSON', { request: { requested_by: 'other' } }],
    ['wrong PR number', { request: { pr_number: 7 } }],
    ['wrong requested PR head', { request: { reviewed_pr_head_sha: OTHER } }],
    ['wrong requested merge SHA', { request: { pr_merge_commit_sha: OTHER } }],
    ['unmerged PR', { prUnmerged: true }],
    ['PR still open', { prOpen: true }],
    ['PR still draft', { prDraft: true }],
    ['PR non-main base', { prWrongBase: true }],
    ['PR actual head changed', { prWrongHead: true }],
    ['PR actual merge commit changed', { prWrongMerge: true }],
    ['no numerical main push', { numericalMissing: true }],
    ['wrong CI run SHA', { wrongRunSha: true }],
  ];
  for (const [name, overrides] of negatives) {
    await assert.rejects(() => scenario(overrides), undefined, name);
    checks++;
  }

  const workflow = fs.readFileSync(path.resolve(__dirname,
    '../../.github/workflows/dispatch-reviewed-release.yml'), 'utf8');
  assert.match(workflow, /^on:\s*\n\s{2}workflow_dispatch:/m); checks++;
  assert.doesNotMatch(workflow, /^\s{2}push:/m); checks++;
  assert.match(workflow, /request_commit_sha:/); checks++;
  assert.match(workflow, /ref:\s*\$\{\{\s*steps\.main\.outputs\.main_sha\s*\}\}/); checks++;
  assert.match(workflow, /scripts\/dispatch-reviewed-main-request\.cjs/); checks++;
  assert.doesNotMatch(workflow, /fs\.readFileSync\(['"]\.github\/release-request\.json/); checks++;
  console.log(`Trusted-main reviewed request dispatch: ${checks} checks, 0 failures`);
})().catch(error => { console.error(error); process.exitCode = 1; });
