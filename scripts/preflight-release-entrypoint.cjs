// Fail-closed preflight for EVERY release.yml entrypoint: manual or v* tag.
// This performs no dispatch, tagging, publication, or file mutation.
const { verifyReviewedReleasePrerequisites } = require('./dispatch-reviewed-release.cjs');
const { verifyLandingPR, loadImmutableRequest } = require('./dispatch-reviewed-main-request.cjs');

const OWNER = 'cheshmakzanoon-ops';
const REPO = 'yamaha-cs80';
const REQUEST_BRANCH = 'heads/codex/cs80-release-dispatch';
const RELEASE_WORKFLOW = 'release.yml';
const SEMVER_TAG_REF = /^refs\/tags\/v[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?$/;

function requireCondition(value, error) {
  if (!value) throw new Error(error);
}

function validSha(value) {
  return typeof value === 'string' && /^[a-f0-9]{40}$/.test(value);
}

async function peelGitTag(github, owner, repo, fullRef) {
  requireCondition(SEMVER_TAG_REF.test(fullRef), 'Release tag must be a vMAJOR.MINOR.PATCH semver ref');
  const ref = fullRef.slice('refs/'.length);
  let object = (await github.rest.git.getRef({ owner, repo, ref })).data.object;
  // Lightweight tag -> commit; annotated tag -> one or more tag objects.
  for (let depth = 0; depth < 5 && object && object.type === 'tag'; depth++) {
    object = (await github.rest.git.getTag({ owner, repo, tag_sha: object.sha })).data.object;
  }
  requireCondition(object && object.type === 'commit' && validSha(object.sha),
    'Release tag does not peel to exactly one full source commit SHA');
  return object.sha;
}

async function resolveRequestCommit(github, owner, repo, eventName, requestSha) {
  if (eventName === 'workflow_dispatch') {
    requireCondition(validSha(requestSha),
      'Manual release requires an exact approved request commit input');
    return requestSha;
  }
  requireCondition(requestSha === undefined || requestSha === null || requestSha === '',
    'Tag push must not supply an alternate request commit input');
  // Tag events have no dispatch input. Freeze one exact request-branch HEAD
  // commit for this run; loadImmutableRequest rechecks it and rejects code
  // changes relative to the approved S. The owner-tag actor remains required.
  const head = (await github.rest.git.getRef({ owner, repo,
    ref: REQUEST_BRANCH })).data.object;
  requireCondition(head && head.type === 'commit' && validSha(head.sha),
    'Tag release request branch has no valid commit tip');
  return head.sha;
}

async function verifyReleaseEntrypoint({ github, context, core, checkoutSha, requestSha }) {
  const { owner, repo } = context.repo;
  requireCondition(owner === OWNER && repo === REPO && context.actor === OWNER,
    'Only the repository owner may release from the reviewed CS-80 repository');
  requireCondition(validSha(checkoutSha), 'Release preflight checkout SHA must be a full commit');
  requireCondition(Number.isInteger(context.runId) && context.runId > 0,
    'Release run identity unavailable for duplicate-run protection');
  if (context.eventName === 'workflow_dispatch') {
    requireCondition(context.ref === 'refs/heads/main',
      'Manual release must dispatch on refs/heads/main');
    requireCondition(context.sha === checkoutSha,
      'Manual release event and checked-out main source differ');
  } else if (context.eventName === 'push') {
    const peeled = await peelGitTag(github, owner, repo, context.ref);
    requireCondition(context.sha === peeled,
      'Release tag push event source differs from its current peeled commit');
    requireCondition(peeled === checkoutSha,
      'Release tag does not point to the exact built source commit');
  } else {
    throw new Error('Only reviewed main workflow_dispatch or a validated v* tag push may release');
  }

  const reviewedRequestSha = await resolveRequestCommit(github, owner, repo,
    context.eventName, requestSha);
  const request = await loadImmutableRequest(github, owner, repo,
    reviewedRequestSha, checkoutSha);
  requireCondition(request.requested_by === context.actor,
    'Immutable release request owner identity differs from authenticated release actor');
  // Direct manual and tag events are independent of the reviewed dispatcher;
  // they must prove the SAME merged PR6 source identity, not just green CI.
  await verifyLandingPR(github, owner, repo, request);
  const { expected, validated } = await verifyReviewedReleasePrerequisites({ github, context, core, request });
  requireCondition(checkoutSha === expected, 'Release checkout is not the approved current-main SHA');

  // The dispatcher already prevents duplicate workflow_dispatches. Check
  // direct manual and tag events too, ignoring this specific in-progress run.
  const runs = (await github.rest.actions.listWorkflowRuns({ owner, repo,
    workflow_id: RELEASE_WORKFLOW, head_sha: expected, per_page: 100 })).data.workflow_runs;
  requireCondition(Array.isArray(runs) && runs.length < 100,
    'Release history is missing or paginated; duplicate-run status cannot be proved');
  const previous = runs.find(run => run.id !== context.runId
    && run.head_sha === expected
    && (run.status !== 'completed' || run.conclusion === 'success'));
  requireCondition(!previous, 'Release already active or successful for the approved main source');
  core.info(`Release entrypoint validated: event=${context.eventName}, exact main=${expected}, reviewed source=${validated}, immutable request=${reviewedRequestSha}`);
  return { authorized: true, expected_main_sha: expected, validated_sha: validated,
    event: context.eventName, request_branch: REQUEST_BRANCH,
    request_commit_sha: reviewedRequestSha };
}

module.exports = verifyReleaseEntrypoint;
