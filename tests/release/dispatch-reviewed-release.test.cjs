const assert = require('node:assert/strict');
const dispatch = require(process.env.CS80_DISPATCH_MODULE || '../../scripts/dispatch-reviewed-release.cjs');
const A = 'a'.repeat(40), B = 'b'.repeat(40), C = 'c'.repeat(40);
let checks = 0;
async function fixture(overrides = {}) {
  const calls = [];
  const request = { expected_main_sha: B, validated_sha: A, ...(overrides.request || {}) };
  const github = { rest: {
    git: { getRef: async () => ({ data: { object: { sha: overrides.main || B } } }) },
    repos: { compareCommits: async () => ({ data: { status: 'ahead', merge_base_commit: { sha: A },
      files: [{ filename: 'docs/screenshots/hardware.png' }], ...(overrides.comparison || {}) } }) },
    actions: {
      listWorkflowRuns: async ({ workflow_id }) => ({ data: { workflow_runs: workflow_id === 'release.yml'
        ? (overrides.existing ? [{ head_sha: B, head_branch: 'main', status: 'in_progress', html_url: 'release-existing' }] : [])
        : [{ head_sha: A, head_branch: 'main', status: 'completed', conclusion: workflow_id === overrides.failed ? 'failure' : 'success', html_url: workflow_id, ...(overrides.run || {}) }] } }),
      createWorkflowDispatch: async args => { calls.push(args); }
    }
  } };
  const result = await dispatch({ github, context: { repo: { owner: 'test', repo: 'cs80' } }, core: { info() {} }, request });
  return { result, calls };
}
(async () => {
  const accepted = await fixture();
  assert.equal(accepted.result.dispatched, true); ++checks;
  assert.deepEqual(accepted.calls, [{ owner: 'test', repo: 'cs80', workflow_id: 'release.yml', ref: 'main' }]); ++checks;
  for (const overrides of [
    { request: { expected_main_sha: 'short' } }, { main: C },
    { comparison: { status: 'diverged' } }, { comparison: { merge_base_commit: { sha: C } } },
    { comparison: { files: [{ filename: 'src/core/Engine.cpp' }] } },
    // A rename has both a new and an old path: S must not remove production
    // code just because the destination is under docs/screenshots/.
    { comparison: { files: [{ filename: 'docs/screenshots/Engine.png',
      status: 'renamed', previous_filename: 'src/core/Engine.cpp' }] } },
    { comparison: { files: [{ filename: 'docs/screenshots/Engine.png',
      status: 'renamed' }] } },
    { comparison: { files: [] } }, { comparison: { files: Array(300).fill({ filename: 'docs/screenshots/test.png' }) } },
    { failed: 'build.yml' }, { failed: 'dsp-measurements.yml' }, { failed: 'screenshots.yml' },
    { run: { head_branch: 'feature' } }, { run: { status: 'in_progress' } }, { run: { head_sha: C } }
  ]) { await assert.rejects(() => fixture(overrides)); ++checks; }
  const existing = await fixture({ existing: true });
  assert.equal(existing.result.dispatched, false); ++checks;
  assert.equal(existing.calls.length, 0); ++checks;
  console.log(`Reviewed release dispatch: ${checks} checks, 0 failures`);
})().catch(error => { console.error(error); process.exitCode = 1; });
