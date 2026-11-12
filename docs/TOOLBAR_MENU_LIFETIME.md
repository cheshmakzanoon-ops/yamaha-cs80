# Studio toolbar menu lifetime

## Scope

The Studio toolbar's menu callbacks are valid only while the toolbar and the
`Actions` wiring that built the menu remain current. This includes every normal
menu command and all display-size submenu choices. It does not change DSP,
parameters, preset recall, menu labels, enabled/ticked flags or the callback
chosen when a menu is built.

## Deferred action ownership

Pinned JUCE 8.0.11 (`c352e2489000ffeb9e397051377d75eef2efe641`) copies a selected
`PopupMenu::Item::action` to `MessageManager::callAsync` after menu dismissal
(`juce_PopupMenu.cpp`, `hide()`). The toolbar previously copied editor callbacks
directly into these actions. Its dismissal callback already used a `SafePointer`,
but that guarded only the later refresh, not the separately queued item action.

Each emitted action now captures a toolbar `SafePointer`, the current Actions
generation and a value copy of its selected callback. `setActions()` advances
the generation, including when clearing the callbacks. Dispatch first checks
that the toolbar still exists and the generation matches. Old actions are
discarded rather than redirected to replacement callbacks. After calling the
captured callback, the wrapper does not touch the toolbar: the callback may
destroy it or replace its Actions. The existing guarded dismissal refresh is
unchanged.

## Regression

`cs80_toolbar_menu_lifetime_tests`, or
`CS80Snapshots --toolbar-menu-lifetime-tests`, constructs the real toolbar and
reads actions from its real menu. It retains and invokes action copies directly,
with counters that outlive the components. It covers:

- All 13 ordinary command IDs and all five display-size choices.
- Live invocation, zoom arguments and selected-zoom tick flags.
- Replacing or clearing Actions before dispatch, then invoking a fresh menu.
- Destroying the toolbar before dispatch, including a destroyed menu object.
- Preserved disconnected/disabled menu-entry rules.
- Normal and zoom callbacks that destroy the toolbar themselves.
- A callback that replaces Actions while it is executing.

With production code unchanged, the controlled regression recorded 57 stale
callback failures out of 149 checks. With the guard, all 149 checks passed, as
did 18 focused/adjacent CTest suites in a fresh Linux build.

The test is also part of the full snapshot self-test. These are offscreen
component/callback checks, with no native popup or desktop peer. They do not
qualify physical menu selection, platform message-queue timing, a DAW or a
release build. No test callback dereferences a deleted editor or toolbar.
