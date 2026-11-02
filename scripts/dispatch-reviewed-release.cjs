// Release only a reviewed main revision. The user/maintainer requests a run by
// committing .github/release-request.json on codex/cs80-release-dispatch.
// GITHUB_TOKEN can trigger workflow_dispatch: https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/trigger-a-workflow
async function verifyReviewedReleasePrerequisites({ github, context, core, request }) {
  const { owner, repo } = context.repo;
  const expected = request.expected_main_sha;
  const validated = request.validated_sha;
  if (![expected, validated].every(x => typeof x === 'string' && /^[0-9a-f]{40}$/.test(x)))
    throw new Error('Both requested revisions must be full commit SHAs');

  const main = (await github.rest.git.getRef({ owner, repo, ref: 'heads/main' })).data.object.sha;
  if (main !== expected) throw new Error('Main moved after the release request; review its new head');

  if (validated !== expected) {
    const comparison = (await github.rest.repos.compareCommits({ owner, repo, base: validated, head: expected })).data;
    if (comparison.status !== 'ahead' || comparison.merge_base_commit.sha !== validated)
      throw new Error('The validated revision must be an ancestor of main');
    const screenshotPath = value => typeof value === 'string'
      && value.startsWith('docs/screenshots/');
    if (!comparison.files || comparison.files.length >= 300 || comparison.files.length === 0
        || comparison.files.some(file => !screenshotPath(file.filename)
          || ((file.status === 'renamed' || file.previous_filename !== undefined)
            && !screenshotPath(file.previous_filename))))
      throw new Error('Only refreshed screenshot assets may follow the validated revision');
  }

  // Numerical qualification is *additive*: the existing three exact-main
  // CI checks and screenshot-only S ancestry remain in force. This fourth
  // workflow must run the registered full-audit score and held/idle CPU gate
  // on validated source M; a green full-audit *capture* alone cannot pass it.
  const required = ['build.yml', 'dsp-measurements.yml', 'screenshots.yml',
    'release-numerical-qualification.yml'];
  for (const workflow_id of required) {
    const runs = (await github.rest.actions.listWorkflowRuns({ owner, repo, workflow_id,
      head_sha: validated, event: 'push', per_page: 100 })).data.workflow_runs;
    const run = runs.find(run => run.head_sha === validated && run.head_branch === 'main');
    if (!run || run.status !== 'completed' || run.conclusion !== 'success')
      throw new Error(`Latest main ${workflow_id} run has not succeeded for ${validated}`);
    core.info(`Validated ${workflow_id}: ${run.html_url}`);
  }
  return { expected, validated };
}

async function dispatchReviewedRelease({ github, context, core, request }) {
  const { expected } = await verifyReviewedReleasePrerequisites({ github, context, core, request });
  const { owner, repo } = context.repo;

  // A retry of this request must not produce another installer run.
  const releases = (await github.rest.actions.listWorkflowRuns({ owner, repo,
    workflow_id: 'release.yml', head_sha: expected, event: 'workflow_dispatch', per_page: 100 })).data.workflow_runs;
  const existing = releases.find(run => run.head_sha === expected && run.head_branch === 'main'
    && (run.status !== 'completed' || run.conclusion === 'success'));
  if (existing) {
    core.info(`Release already dispatched: ${existing.html_url}`);
    return { dispatched: false, existing: existing.html_url };
  }
  await github.rest.actions.createWorkflowDispatch({ owner, repo, workflow_id: 'release.yml', ref: 'main' });
  core.info(`Dispatched release.yml on main at ${expected}`);
  return { dispatched: true, sha: expected };
}

module.exports = dispatchReviewedRelease;
module.exports.verifyReviewedReleasePrerequisites = verifyReviewedReleasePrerequisites;
