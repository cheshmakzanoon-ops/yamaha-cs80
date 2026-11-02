// Preregistered RED-first tests for an *additional*, exact-M numerical pass
// before reviewed release dispatch; never replace its existing three checks.
const assert = require('node:assert/strict');
const dispatch = require('../../scripts/dispatch-reviewed-release.cjs');

const M = 'a'.repeat(40), S = 'b'.repeat(40), OTHER = 'c'.repeat(40);
const NUMERICAL = 'release-numerical-qualification.yml';
let checks = 0;

async function scenario(overrides = {}) {
  const calls = [];
  const lookups = [];
  const request = { validated_sha: M, expected_main_sha: S };
  const github = { rest: {
    git: { getRef: async () => ({ data: { object: { sha: S } } }) },
    repos: { compareCommits: async () => ({ data: { status: 'ahead', merge_base_commit: { sha: M },
      files: [{ filename: 'docs/screenshots/final.png' }] } }) },
    actions: {
      listWorkflowRuns: async args => {
        lookups.push(args);
        const { workflow_id } = args;
        if (workflow_id === 'release.yml') return { data: { workflow_runs: [] } };
        if (workflow_id === NUMERICAL && overrides.missingNumerical)
          return { data: { workflow_runs: [] } };
        return { data: { workflow_runs: [{
          head_sha: workflow_id === NUMERICAL && overrides.staleNumerical ? OTHER : M,
          head_branch: workflow_id === NUMERICAL && overrides.prNumerical ? 'feature' : 'main',
          status: workflow_id === NUMERICAL && overrides.pendingNumerical ? 'in_progress' : 'completed',
          conclusion: workflow_id === NUMERICAL && overrides.failedNumerical ? 'failure' : 'success',
          html_url: workflow_id,
        }] } };
      },
      createWorkflowDispatch: async args => calls.push(args),
    },
  } };
  const result = await dispatch({ github, context: { repo: { owner: 'review', repo: 'cs80' } },
    core: { info() {} }, request });
  return { result, calls, lookups };
}

(async () => {
  for (const failure of ['missingNumerical', 'staleNumerical', 'prNumerical',
                         'pendingNumerical', 'failedNumerical']) {
    await assert.rejects(() => scenario({ [failure]: true }), /numerical|qualification/i);
    checks++;
  }
  const good = await scenario();
  assert.equal(good.result.dispatched, true); checks++;
  assert.deepEqual(good.calls, [{ owner: 'review', repo: 'cs80', workflow_id: 'release.yml', ref: 'main' }]);
  checks++;
  const numerical = good.lookups.find(lookup => lookup.workflow_id === NUMERICAL);
  assert.ok(numerical, 'the fourth workflow must be checked'); checks++;
  assert.equal(numerical.head_sha, M); checks++;
  assert.equal(numerical.event, 'push'); checks++;
  const required = ['build.yml', 'dsp-measurements.yml', 'screenshots.yml', NUMERICAL];
  assert.ok(required.every(id => good.lookups.some(lookup => lookup.workflow_id === id)));
  checks++;
  console.log(`Additive numerical dispatch: ${checks} checks, 0 failures`);
})().catch(error => { console.error(error); process.exitCode = 1; });
