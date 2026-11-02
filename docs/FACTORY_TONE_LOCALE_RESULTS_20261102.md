# Factory-tone locale result record (2026-11-02)

Registered test-only head80907b1, based on9ba789, ran at14:34:13 UTC:
**296 checks,13 ordinary failures, exit1**. The C controls pass; the German
fixture exposes actual session/preset/A-B V2 loss, wrong source readback,
decimal/hex-literal loss and locale-dependent acceptance of1,0. The original
C/German/French3-check diagnostic remains separately preserved.

Repair scope: only the new factory-tone revision decoder. A private RAII
C-numeric locale wraps platform strtod_l/_strtod_l, leaves the caller locale
unchanged and fails closed to V1 if allocation fails. Full-consumption and
the existing exact-one rule remain. The new revision is decoded directly
rather than first traversing JUCE's unrelated generic string-to-float path.

The old ENGINE/PURE decoder is intentionally untouched in this increment.
Windows/macOS compilation and native locale verification remain pending.
Post-change results have not yet been recorded.
