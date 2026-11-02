// Fail-closed preflight for the ONLY proposed release.yml entrypoint:
// owner-only workflow_dispatch on approved current main.
// This performs no dispatch, tagging, publication, or file mutation.
const { verifyReviewedReleasePrerequisites } = require('./dispatch-reviewed-release.cjs');
const { verifyLandingPR, loadImmutableRequest } = require('./dispatch-reviewed-main-request.cjs');

const OWNER = 'cheshmakzanoon-ops';
const REPO = 'yamaha-cs80';
const RELEASE_WORKFLOW = 'release.yml';

function requireCondition(value, error) {
  if (!value) throw new Error(error);
}

function validSha(value) {
  return typeof value === 'string' && /^[a-f0-9]{40}$/.test(value);
}

async function verifyReleaseEntrypoint({ github, context, core, checkoutSha, requestSha }) {
  const { owner, repo } = context.repo;
  requireCondition(owner === OWNER && repo === REPO && context.actor === OWNER,
    'Only the repository owner may release from the reviewed CS-80 repository');
  requireCondition(validSha(checkoutSha), 'Release preflight checkout SHA must be a full commit');
  requireCondition(Number.isInteger(context.runId) && context.runId > 0,
    'Release run identity unavailable for duplicate-run protection');
  requireCondition(context.eventName === 'workflow_dispatch'
    && context.ref === 'refs/heads/main',
    'Only owner-reviewed manual workflow_dispatch on main may build installers');
  requireCondition(context.sha === checkoutSha,
    'Manual release event and checked-out main source differ');
  requireCondition(validSha(requestSha),
    'Manual installer release requires an exact approved request commit SHA');

  const request = await loadImmutableRequest(github, owner, repo,
    requestSha, checkoutSha);
  requireCondition(request.requested_by === context.actor,
    'Immutable release request owner identity differs from authenticated release actor');
  // This direct manual path is independent of the reviewed dispatcher, so it
  // must prove the same merged PR6 identity and four exact-main checks.
  await verifyLandingPR(github, owner, repo, request);
  const { expected, validated } = await verifyReviewedReleasePrerequisites({ github, context, core, request });
  requireCondition(checkoutSha === expected, 'Release checkout is not the approved current-main SHA');

  // The dispatcher already prevents duplicate workflow_dispatches. Check
  // direct manual events too, ignoring this specific in-progress run.
  const runs = (await github.rest.actions.listWorkflowRuns({ owner, repo,
    workflow_id: RELEASE_WORKFLOW, head_sha: expected, per_page: 100 })).data.workflow_runs;
  requireCondition(Array.isArray(runs) && runs.length < 100,
    'Release history is missing or paginated; duplicate-run status cannot be proved');
  const previous = runs.find(run => run.id !== context.runId
    && run.head_sha === expected
    && (run.status !== 'completed' || run.conclusion === 'success'));
  requireCondition(!previous, 'Release already active or successful for the approved main source');
  core.info(`Owner-only manual release entrypoint validated: exact main=${expected}, reviewed source=${validated}, immutable request=${requestSha}`);
  return { authorized: true, expected_main_sha: expected, validated_sha: validated,
    event: context.eventName, request_branch: 'codex/cs80-release-dispatch',
    request_commit_sha: requestSha };
}

module.exports = verifyReleaseEntrypoint;
