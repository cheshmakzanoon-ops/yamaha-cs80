// Fail-closed preflight for EVERY release.yml entrypoint: manual or v* tag.
// This performs no dispatch, tagging, publication, or file mutation.
const { verifyReviewedReleasePrerequisites } = require('./dispatch-reviewed-release.cjs');

const REQUEST_BRANCH = 'codex/cs80-release-dispatch';
const REQUEST_PATH = '.github/release-request.json';
const RELEASE_WORKFLOW = 'release.yml';
const SEMVER_TAG_REF = /^refs\/tags\/v[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?$/;

function requireCondition(value, error) {
  if (!value) throw new Error(error);
}

function validSha(value) {
  return typeof value === 'string' && /^[a-f0-9]{40}$/.test(value);
}

async function readReviewedReleaseRequest(github, owner, repo) {
  // Callers cannot supply a free-form request input to bypass review.
  const { data } = await github.rest.repos.getContent({ owner, repo,
    path: REQUEST_PATH, ref: REQUEST_BRANCH });
  requireCondition(data && !Array.isArray(data) && data.encoding === 'base64'
    && typeof data.content === 'string' && data.content.length > 0,
  'Reviewed release-request.json missing or not a base64 file on the request branch');
  let request;
  try {
    request = JSON.parse(Buffer.from(data.content.replace(/\s/g, ''), 'base64').toString('utf8'));
  } catch (error) {
    throw new Error(`Reviewed release request cannot be decoded: ${error.message}`);
  }
  requireCondition(request && typeof request === 'object' && !Array.isArray(request),
    'Reviewed release request is not an object');
  return request;
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

async function verifyReleaseEntrypoint({ github, context, core, checkoutSha }) {
  const { owner, repo } = context.repo;
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

  const request = await readReviewedReleaseRequest(github, owner, repo);
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
  core.info(`Release entrypoint validated: event=${context.eventName}, exact main=${expected}, reviewed source=${validated}`);
  return { authorized: true, expected_main_sha: expected, validated_sha: validated,
    event: context.eventName, request_branch: REQUEST_BRANCH };
}

module.exports = verifyReleaseEntrypoint;
