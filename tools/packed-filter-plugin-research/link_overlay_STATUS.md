# Map-only CMake overlay: inert preparation controls

`link_overlay.cmake` is a Linux GNU/Clang overlay for `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`, deferred to the end of the root directory. It adds only these private target link options:

- `CS80Reference_VST3`: `-Wl,-Map,<canonical-build>/CS80Reference_VST3.research.map,--cref`
- `CS80Reference_Standalone`: `-Wl,-Map,<canonical-build>/CS80Reference_Standalone.research.map,--cref`

No compile option, definition, optimization, environment, source, dependency or test setting is changed. Other targets receive no new options. Exact target names are fixed, not caller-configurable. Missing/alias/imported/static/interface targets, double inclusion, noncanonical/symlink build paths and paths with comma/list/generator-expression delimiters are rejected. Existing map/print-map/cref options in visible linker/compiler flag variables, directory link properties, build/imported target link properties and configuration-specific legacy link flags are rejected. Traversal includes subdirectories and interface/transitive target properties. This is conservative: conflicting flags on unrelated targets also reject the configure. Attached `-Mapfilename`/`--Mapfilename` prefixes are rejected even when the selected GNU linker rejects them as ambiguous; ordinary `-MD`/`-MMD` dependency flags are not misclassified as maps.

## Actual inert controls

28 controls pass with explicit `CS80_CMAKE` and `CS80_NINJA`. The fixture creates only three tiny C++ targets, configures and compiles/links them, and NEVER executes any resulting binary. No CS80 source is configured/built/run.

The main positive control first configures/builds without the overlay, recreates the same canonical build directory, then configures/builds with the same overlay path. `compile_commands.json` must remain byte-for-byte identical. Actual verbose build output is tokenized, not merely inferred from CMake properties. Each selected target's final linker command must differ by exactly one expected map option; removing it must reproduce the entire baseline command. The unselected target's entire linker command remains identical. Both map files must have been absent before build, newly written after build started, nonempty and contain the cross-reference table; exactly two research maps exist. Separate stock/candidate-labeled inert passes use the identical overlay and canonical paths and assert identical compile records and actual link commands. Space-containing paths also compile successfully. Negative controls include target/configuration/directory/global/compiler/link-library/interface/genex map flags, subdirectory flag scope, custom configuration flags, target absence/type and path/double-inclusion errors.

Tools actually exercised:
- CMake 3.31.6, SHA256 `57452fa01ed0f69552c06d3e25edd0d9375347b982dfb709393526cb45396d6b`.
- Existing Ninja `1.13.0.git.kitware.jobserver-pipe-1`, SHA256 `696f9628a79d9ce50314cf9556d7cd1a1d1ec52b8fd52828f6f9db1719565b67`.

No tool was installed. These are local control observations, not frozen qualification-tool approval. The test class explicitly skips when either tool path is absent; a skipped class is not evidence the controls passed.

## Mandatory integration gates

The overlay does not itself qualify production linking or seal project/runtime identity. Use the same hashed overlay at the SAME canonical path for both arms. Independently freeze source, compiler/toolchain, environment, CMake/Ninja, dependencies, configure arguments and target roster. Compare actual compile and final linker command inventories against the unmodified configuration; require exactly the two expected map changes and no other build semantic differences. The overlay cannot interpret arbitrary opaque response files, custom compiler/linker wrappers, or generator expressions that dynamically construct a hidden flag. Later deferred project commands can also change settings after this hook. The frozen trusted project plus actual resolved final argv/response-file audit must reject these cases; configuration-time scanning alone is insufficient.

Enforce clean final relinks, absent/fresh maps and exact output/module bindings in the coordinator. This overlay does not delete stale files, enforce relink, collect linker evidence, validate map provenance, upload artifacts, or launch a workflow. No real qualification launch, runtime equivalence, timing claim or production admission follows from these controls.
