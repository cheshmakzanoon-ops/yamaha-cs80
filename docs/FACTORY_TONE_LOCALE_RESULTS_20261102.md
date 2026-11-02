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

## Positive, removed-guard and allocation-failure evidence

On source740854f1 (test/tool tiped5a67e), the registered C/German fixture passed
**296/296 checks** at14:40:16 UTC. Restoring plain std::strtod in the actual
production decoder reproduced exactly **13 ordinary failures in296 checks**.
The unchanged positive executable was rehashed and passed again after testing.

A separate isolated compiled fault injection forces the POSIX locale handle
to nullptr. Its **14/14** checks establish V1 fallback from a live/saved V2
state, actual V1 source buses, unchanged caller locale and clean destruction.
This simulates allocation failure; it does not claim the OS actually ran out
of memory. No shipping runtime fault hook was added.

API declarations and ownership were checked against primary documentation:
[Microsoft locale creation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/create-locale-wcreate-locale)
does not change the current locale and pairs with
[_free_locale](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/free-locale).
[_strtod_l](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/strtod-strtod-l-wcstod-wcstod-l)
accepts an explicit locale and decimal/hexadecimal forms. Apple documents
[newlocale](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/newlocale.3.html),
[strtod_l](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/strtod_l.3.html)
and [freelocale](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/freelocale.3.html)
in xlocale.h. Pinned JUCE8.0.11 StandardHeader.h includes xlocale.h on Apple;
the processor explicitly includes locale.h/cstdlib. These checks do not
substitute for native Windows/macOS builds, which remain pending.
