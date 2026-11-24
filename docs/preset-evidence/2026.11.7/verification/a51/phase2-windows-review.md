# A51 Windows public route — phase 2

## Latest phase 2: Windows passed; two of five public routes verified

The actual Windows x64 installation artifact **14925251949** passed an
unauthenticated full GET from <https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14925251949.zip>. It returned **HTTP 200**,
no partial `Content-Range`, **6,409,200 bytes**, and SHA-256
`8f8f885c72910a0af02055036203a6a2e0aa96a9390adce7c1ec480243ba0feb`, exactly matching authenticated API metadata. The observation
ran **2026-11-24T14:05:57.202963+00:00** through **2026-11-24T14:06:19.194618+00:00**. Its first
transport succeeded; zero failed Windows public transports were observed.
The stream was hashed without retaining another ZIP or signed redirect queries.

A51 checked the supplied A46 qualification pin `a86ab93d18fb390359f60be8a50ed9cc09b34c9b70aab8922c23581fa455c048` and authenticated
retrieval pin `b914626a64b3dec3305a0228540f4976d908570741bc6b131dcd6236e8fd9938`, all retained raw metadata/log digests, and all
referenced A46 report identities. All 17 externally qualified Windows gates remain
accepted. The exact original install upload, ID/name/bytes/hash, timestamp, API
step, source `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, tree `17115913cbb29157bd00539485bcc5f38b57219c`, run `45397189410`, producing attempt `1`,
producer `138455976256`, and validator consumer `138483772246` are bound. No product
tests were repeated. The helper's limited attempt-binding flag remains unchanged;
the separate binding receipt supplies the original-upload evidence.

Both verified artifacts currently report `expired: false` and the same actual
expiry: **February 22, 2027, 12:49:05 UTC**, or **8:49:05 a.m. AST
(UTC−04:00), America/Halifax**. The final five-download earliest expiry stays
unset. Prior ARM GET/metadata/qualification receipts were verified by hash and
preserved unchanged, with the original ARM observation time retained.

The combined mapping has **two verified routes and three pending targets**:
Intel Mac, Linux, and audition. Per the coordinator/A49 handoff, original Intel
attempt 1 failed a UI meter/timer assertion; the unchanged full-job rerun is a
recommendation, and the prior API 403 did not start a rerun. This is separately
recorded context, not an A51 product result or failed public transport. No Intel
replacement ID or successful attempt is inferred. Each later attempt must retain
its own source, producer, and transport evidence.

The [same-run Artifacts fallback](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45397189410#artifacts)
still depends on the same retained artifacts, may require sign-in, and cannot
recover expiry or deletion. API expiry is not guaranteed continuous availability;
workflow artifacts are not permanent release assets. No global release,
publisher signing/notarization, or listening acceptance is claimed.

- [Combined ARM and Windows mapping](../../ci/source-ee0dabca/public-downloads/download-mapping-phase2.json).
- [Actual Windows full GET](../../ci/source-ee0dabca/public-downloads/windows-x86_64-full-get-0001.json).
- [Windows identity and original-attempt binding](phase2-windows-binding.json).
- [Windows commands](phase2-command-receipts.json).
- [Windows binding verifier](phase2-windows-binding.py).

The following phase 1 record is historical and preserves its original scope.

