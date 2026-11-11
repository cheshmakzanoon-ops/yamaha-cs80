# Parameter state mirror feedback

## Reproduced defect

The undo/state layer mirrors the current parameter value into an APVTS ValueTree. Before this correction, APVTS treated that mirror as a new parameter edit. A newer parameter update arriving between the snapshot and the APVTS tree listener could consequently be overwritten by the older snapshot.

The regression controls this interleaving without relying on a scheduler: a child ValueTree listener publishes a newer parameter value during the mirror write, before the root APVTS listener sees that same write. The exact pre-notifier `6fc5354d` implementation fails 18 of 79 checks. The test also exercises a genuine newer tree edit, recursive synchronization, four parameter shapes, owner/child/parent/property identities, temporary handles, nested and inactive scopes, thread isolation, and ordinary undo/redo.

This is a separately reproduced correctness defect. It does not, by itself, prove that the defect caused every intermittent offline VST3 failure observed during the program-display notifier investigation.

## Correction

The initial mirror write is marked with a thread-local scope identifying its APVTS owner, child, parent, property and finite snapshot. The APVTS tree listener ignores only that exact mirror while its stored value still matches the snapshot. A genuine changed tree value is processed normally. A newer parameter callback during the write is queued for the next synchronization instead of being dropped; recursive synchronization does not consume pending work while a write is in progress. Undo/redo replay occurs outside the initial mirror scope. The scope owns ValueTree handles for lifetime safety; this pinned JUCE can allocate while constructing those handles on the message thread when assertions or assertion logging are enabled. This is not an allocation-free mirror or a CPU optimization claim.

JUCE's public API does not provide this specific listener admission hook. The build therefore generates a narrowly adapted copy of the JUCE 8.0.11 audio-processors translation unit and APVTS implementation in the build directory. The original JUCE checkout is not modified. The generator verifies three LF-normalized source hashes and exact replacement anchors, rejects destinations inside the original module, and fails configuration on unknown inputs. There is no APVTS class-layout or public-API change. The generated module replaces, rather than duplicates, the original translation unit.

Six generator tests check hash and anchor rejection, source preservation, output protection, idempotence, CRLF inputs, and C++/Objective-C++ language handling. Objective-C++ generation is tested as text only; no macOS or Windows compilation is claimed.

## Qualification status

After the final source changes, a second affected-target build recompiled the undo implementation and generated JUCE module, relinked the VST3 module and native fixtures, and completed 12 build steps. All 23 selected tests passed in 68.64 seconds, including the 79-check mirror regression, six generator tests, allocation fixture, undo/state fixtures, and all six actual-module rate/mode registrations. An earlier 23-test run used the previous 74-check fixture and is not the final-source qualification. Twenty subsequent isolated runs of the unchanged 44.1 kHz offline VST3 test all passed, with no tolerance edits. The fixture SHA-256 was `f26fa76f7fd9c4e05fd8d6570883932eb395a22ba5cdc8d93477a399a860ddb3`; the corrected module was `416be52940e913ad009dbeb1fce7536b443098d43508094bc5b363ed722e8c61`. This is a bounded repeat result, not proof that an intermittent failure is impossible. An independent final execution of the same 23-test selection also passed, in 69.96 seconds. The earlier failures remain part of the record; this bounded qualification does not establish that every earlier offline failure had the same cause. The earlier exact pre-notifier module passed ten isolated 44.1 kHz offline repeats; that limited control sample does not establish absence of an intermittent failure.

The controlled test establishes admission of this specific synchronous listener interleaving. It does not make arbitrary concurrent ValueTree mutation safe or establish an atomic transaction across all parameters.

No tolerance was relaxed. No full-suite, external-DAW, GUI, live MTS service, CPU deadline, or lock-free callback qualification follows from this change.
