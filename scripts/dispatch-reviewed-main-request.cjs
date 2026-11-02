// Maintainer-only, default-main release request entrypoint. The caller supplies
// a full request COMMIT SHA; the request branch is JSON data, never code.
// This helper runs exclusively from a SHA-pinned checkout of reviewed main.
// It does not bypass the existing 3+1 exact-main CI or screenshot ancestry.
const dispatchReviewedRelease = require('./dispatch-reviewed-release.cjs');

const OWNER = 'cheshmakzanoon-ops';
const REPO = 'yamaha-cs80';
const REQUEST_BRANCH = 'heads/codex/cs80-release-dispatch';
const REQUEST_FILE = '.github/release-request.json';
const LANDING_PR = 6;

function requireCondition(value, message) {
  if (!value) throw new Error(message);
}

function fullSha(value) {
  return typeof value === 'string' && /^[0-9a-f]{40}$/.test(value);
}

async function loadImmutableRequest(github, owner, repo, requestSha,
                                    approvedMainSha) {
  const head = (await github.rest.git.getRef({ owner, repo,
    ref: REQUEST_BRANCH })).data.object;
  requireCondition(head && head.type === 'commit' && head.sha === requestSha,
    'Request branch head is not the exact reviewed request commit');

  // Fail closed if the request branch changes executable code, workflow YAML,
  // or any file other than its single JSON request relative to reviewed main.
  const comparison = (await github.rest.repos.compareCommits({ owner, repo,
    base: approvedMainSha, head: requestSha })).data;
  requireCondition(comparison && comparison.status === 'ahead'
    && comparison.merge_base_commit && comparison.merge_base_commit.sha === approvedMainSha
    && Array.isArray(comparison.files) && comparison.files.length === 1
    && comparison.files[0].filename === REQUEST_FILE
    && ['added', 'modified'].includes(comparison.files[0].status)
    && comparison.files[0].previous_filename === undefined,
  'Request commit must change only the JSON file relative to reviewed main');

  const response = (await github.rest.repos.getContent({ owner, repo,
    path: REQUEST_FILE, ref: requestSha })).data;
  requireCondition(response && !Array.isArray(response) && response.type === 'file'
    && response.encoding === 'base64' && typeof response.content === 'string'
    && response.content.length > 0,
  'Exact request commit must contain one base64 JSON file');
  let request;
  try {
    request = JSON.parse(Buffer.from(response.content.replace(/\s/g, ''),
      'base64').toString('utf8'));
  } catch (error) {
    throw new Error(`Request commit JSON invalid: ${error.message}`);
  }
  requireCondition(request && typeof request === 'object' && !Array.isArray(request),
    'Request payload must be an object');
  return request;
}

async function verifyLandingPR(github, owner, repo, request) {
  requireCondition(request.pr_number === LANDING_PR,
    'Release request must identify reviewed PR #6');
  requireCondition(fullSha(request.reviewed_pr_head_sha)
    && fullSha(request.pr_merge_commit_sha),
  'Request must pin full PR head and merge commit identities');
  const pr = (await github.rest.pulls.get({ owner, repo,
    pull_number: LANDING_PR })).data;
  requireCondition(pr && pr.number === LANDING_PR && pr.state === 'closed'
    && pr.merged === true && pr.draft === false
    && pr.base && pr.base.ref === 'main',
  'PR #6 is not merged, non-draft and based on main');
  requireCondition(pr.head && pr.head.sha === request.reviewed_pr_head_sha,
    'Requested PR head differs from reviewed PR #6 head');
  requireCondition(pr.merge_commit_sha === request.pr_merge_commit_sha
    && request.pr_merge_commit_sha === request.validated_sha,
  'Requested validated main commit must be the exact PR #6 merge commit');
}

async function dispatchReviewedMainRequest({ github, context, core, requestSha,
                                              checkoutSha, approvedMainSha }) {
  const { owner, repo } = context.repo;
  requireCondition(owner === OWNER && repo === REPO,
    'Only the reviewed CS-80 repository may dispatch');
  requireCondition(context.eventName === 'workflow_dispatch'
    && context.ref === 'refs/heads/main',
  'Only a maintainer workflow_dispatch on main may request release');
  requireCondition(context.actor === OWNER,
    'Only the repository owner may approve a release request');
  requireCondition(fullSha(requestSha), 'Request commit input must be a full SHA');
  requireCondition(fullSha(approvedMainSha) && fullSha(checkoutSha)
    && context.sha === approvedMainSha && checkoutSha === approvedMainSha,
  'Main event, API-approved main and checked-out guard source must match');

  const main = (await github.rest.git.getRef({ owner, repo,
    ref: 'heads/main' })).data.object;
  requireCondition(main && main.type === 'commit' && main.sha === approvedMainSha,
    'Current main moved after this reviewed dispatch event');

  const request = await loadImmutableRequest(github, owner, repo, requestSha,
    approvedMainSha);
  requireCondition(request.requested_by === context.actor,
    'Request JSON owner identity does not match authenticated maintainer');
  requireCondition(request.expected_main_sha === approvedMainSha,
    'Request main SHA is stale or differs from the protected checkout');
  await verifyLandingPR(github, owner, repo, request);

  // The shared guard still insists on build, DSP, screenshot and numerical
  // successes from validated SHA, exact-main lease and screenshot-only S.
  const result = await dispatchReviewedRelease({ github, context, core, request });
  core.info(`Owner-approved immutable request ${requestSha} reviewed against main ${approvedMainSha}`);
  return result;
}

module.exports = dispatchReviewedMainRequest;
module.exports.loadImmutableRequest = loadImmutableRequest;
module.exports.verifyLandingPR = verifyLandingPR;
