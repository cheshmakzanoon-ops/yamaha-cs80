# A57 — Actual Windows public download checkpoint

**The current Windows public download is verified. The full release and the
other four advertised routes remain pending.**

A normal unauthenticated full GET to
[artifact 14903717361](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14903717361.zip)
returned **HTTP 200** and **6,387,912 bytes**, with SHA-256
`0123b292a7f5fd818efdfc5db87f87e112c48beabf90b3201c79a579a8b64b3f`.
Both values exactly match the fresh authenticated GitHub artifact metadata.
The public read completed on **24 November 2026 at 08:54:23 UTC**.

The artifact identifies source
`c76ad288e02fbeca48f39652bf4fcdd99837f793`, native run **45351731801**, and name
`cs80-windows-install-bundle`. The retained original producer API identifies
job **138299557166**, attempt **1**, with conclusion **success**. Full original
upload ID/digest/time binding and installer/native/validator acceptance remain
A49's separate qualification scope. Public response equality does not replace
those gates.

[Public-route acceptance](windows-public-route-acceptance.json) ·
[Original public GET receipt](windows-attempt1-public-get.json) ·
[Fresh artifact API](windows-attempt1-artifact-api.json) ·
[Run context at the read](windows-attempt1-run-api.json) ·
[Original producer API](windows-attempt1-producer-api.json)

## Executed inspection

The previously prepared [streaming helper](../../../verification/a57-probe-public-artifact.py)
was executed against the actual current artifact and completed successfully.
Its SHA-256 remains
`ca863189b395c2952253de8f4b64538dd045f497db39b2bfcd12b40fd81bc06f`.
All seven frozen A57 checkpoint file identities remain unchanged.

The response was fully hashed without retaining a second Windows ZIP. No
Authorization header, GitHub token or cookies were supplied. Redirects remained
HTTPS; signed query strings were omitted from the receipt. The raw helper
keeps `artifact_attempt_binding_verified: false` and `release_qualified: false`.
Its current `public_route_verified: true` applies only to this artifact at the
recorded observation time.

## Per-artifact attempt mapping

The [partial current mapping](windows-partial-download-map.json) records the
actual Windows ID, hash, public URL and expiry. The original Windows producer
selection is attempt 1. Each later artifact must carry its own selected producer
and actual attempt, with separate Windows/Linux consumer binding. No single
aggregate run-attempt number is applied to all archived products.

This narrowly supersedes the single-attempt assumption in the frozen preparation
contract. The [A58 mixed-attempt requirements](../release-gate-audit/mixed-attempt-supplement.md)
remain authoritative for native acceptance. The Apple Silicon replacement job,
actual attempt and installer ID are unknown here; its failed original job is
not used as an installer or a set of successful replacement validator lanes.
Intel, Linux and audition IDs and public GET receipts are also still pending.

For a public check after a later workflow rerun, supply the run API for the
selected producing attempt to the helper, together with its actual artifact
metadata. The original successful upload record supplies the producing-attempt
proof; fresh aggregate run metadata or a matching artifact name does not.

## Retention and remaining work

This Windows artifact expires at **22 February 2027, 07:55:18 UTC**, which is
**03:55:18 AST** in America/Halifax. This is **not** asserted as the earliest
expiry of the five advertised artifacts. The overall minimum remains null
until actual metadata for all four installers and the audition archive is
available and inspected.

Reactivation must verify the remaining actual public responses, reconcile every
selected ID/hash with its platform or audition acceptance and original upload
binding, and compute the real minimum expiry. The run's
[Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45351731801#artifacts)
remains the fallback while the selected Actions artifacts are retained.

No source, workflow, README, frozen A57 checkpoint, historical evidence or remote
state was changed. This checkpoint is confined to the new downloads directory.
