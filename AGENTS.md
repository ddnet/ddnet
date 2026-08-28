# DDNet coding agent instructions

These instructions apply to the entire repository. They turn the project's contribution rules and recurring maintainer feedback into an operating procedure for AI agents. Treat them as defaults together with [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md), the build configuration, tests, and conventions in the code being changed.

## Priorities

Use this order when requirements compete:

1. Correctness, memory safety, input safety, and preservation of user data.
2. Gameplay, protocol, file-format, demo, map, savegame, and platform compatibility.
3. A focused, reviewable change that follows the affected subsystem's design.
4. Verification with reproducible evidence.
5. Readability and project style.
6. Performance, unless the task is specifically a measured performance problem.

Do not trade a higher priority for a lower one without making the conflict explicit. If the requested result requires a compatibility break or changes established physics, stop and report the exact trade-off instead of choosing policy on behalf of the project.

## Before editing

- Read the nearest relevant code, tests, build files, and documentation. Search for every declaration, caller, serializer, protocol branch, and platform implementation affected by the change; do not patch only the first visible symptom.
- Inspect the worktree before editing. Preserve existing user changes, submodule state, and unrelated untracked files. Never discard or rewrite work outside the task.
- State the intended observable behavior and identify the compatibility surfaces before implementation. For bugs, determine a concrete reproduction or explain why reproduction is unavailable.
- Check whether a substantial feature has already been accepted and is not being implemented elsewhere. If project alignment is unknown, report that prerequisite rather than implementing a speculative feature. Do not open issues, contact maintainers, or create external artifacts unless the user asks.
- Prefer the smallest change that fully fixes the behavior. Do not include opportunistic refactors, renames, dependency upgrades, mass formatting, or cleanup.
- Follow the local subsystem even when older code differs from current general guidance. Do not mechanically modernize adjacent code.

## Implementation discipline

### Correctness and hostile input

- Treat packets, pre-authentication traffic, console commands, maps, demos, skins, savegames, database data, file paths, and platform APIs as potentially malformed or adversarial.
- Validate lengths, indexes, counts, enum values, ranges, allocation sizes, and state transitions before use. Check empty, zero, maximum, truncated, duplicate, unsupported, disconnected, and exhausted-resource cases.
- Bound CPU, memory, decompression, parsing, and response work that an unauthenticated peer can trigger. Do not turn a small packet into unbounded per-client or per-tick work.
- Check integer arithmetic before allocation or indexing. Avoid signed overflow, narrowing, underflow, and undefined shifts. Replacing undefined behavior must preserve established observable behavior when compatibility depends on it.
- Prefer an explicit failure over partially initialized state, corrupted output, or silently changed physics. Saving and serialization must fail atomically when data cannot be represented.
- Propagate errors to a layer that can handle them meaningfully. Do not zero a size, clamp a value, ignore a return value, or continue with fallback data unless that behavior is deliberate and compatible.
- Make ownership and lifetime explicit. Check borrowed pointers, callback captures, optional values, handles, thread handoffs, and every early-return path for leaks, use-after-free, double-free, races, and stale state.

### Compatibility

- Preserve behavior for vanilla 0.6 and 0.7, older DDNet clients and servers, demos, snapshots, existing maps and ranks, skins, savegames, database schemas, and established gameplay.
- Treat protocol message IDs, snapshot object IDs, `GameInfoEx`, capability flags, client-version discovery, tuning, physics, map data, and serialization layouts as public compatibility surfaces.
- Do not send extension data before the peer's version or capability is known. Gate new behavior on the established capability/version mechanism and define the fallback for absent, old, and unknown peers.
- When extending a message or object, verify how older readers handle missing or trailing fields and how newer readers handle old payloads. Do not add a new tiny network message without considering existing extensible messages and wire overhead.
- Test both ends of protocol changes. Include reconnects, mixed client versions, missing capabilities, full ID/player/team pools, maximum counts, and translation between 0.6 and 0.7 paths where relevant.
- Keep client prediction and server authority consistent. A server-side physics change usually requires checking prediction, antiping, demos, teehistorian, and old-server behavior.
- Keep platform fixes scoped to the affected backend and SDK. Do not assume behavior on Windows, Linux/X11/Wayland, macOS, Android, iOS, or Emscripten is interchangeable.

### API and C++ design

- Match [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md), `.clang-format`, and nearby code. `src/base` has its own naming style.
- Use UpperCamelCase for new variables, methods, and classes outside `src/base`. Choose role-specific names such as `ClientId`, `Team`, and `Dummy`, including loop indexes.
- Use only the established prefixes: `m_` members, `g_` globals, `s_` statics, `p` pointers, `a` fixed arrays, `v` vectors, `pfn` C function pointers, and `F` function types. Classes use `C`, interfaces `I`, and enums `E` with SCREAMING_SNAKE_CASE literals.
- Use scoped enums for enumerations and `inline constexpr` values in a named namespace for flags. Avoid new C-style constants and function-like macros.
- Prefer `class`, in-class member initialization, `constexpr`, `nullptr`, standard algorithms, RAII, and supported modern C++. Keep headers self-contained with the direct includes required by their declarations.
- Avoid new global or mutable static state, default arguments, overload sets that could have descriptive names, `goto`, assignments in conditions, and implicit integer-to-boolean tests.
- Success/failure APIs return `bool`, with `true` meaning success. Represent genuine absence with `std::optional` instead of an undocumented sentinel, and check it before access.
- New getters do not use a `Get` prefix. Keep APIs small and explicit about ownership, mutation, units, ranges, and lifetime. Use `const` where it expresses the actual contract.
- Keep hot game-loop and per-client/per-tick paths allocation-free where practical. Reuse storage or use bounded stack storage, but do not sacrifice clarity for an unmeasured micro-optimization.
- Split long expressions and lists at logical boundaries even if clang-format accepts one line. Prefer one item per line when it improves reviewability and `git blame`.
- Use lowercase underscore-separated names for new files and directories.
- Document all public declarations in `src/base` with Javadoc-style Doxygen comments. Elsewhere, document reusable APIs, compatibility decisions, non-obvious invariants, units, and version/limit bump rules in the code rather than only in a handoff message.

### UI, text, and generated data

- Preserve established input semantics, focus, selection, clipboard behavior, and key handling. After handling a shortcut, verify that the same event cannot also fall through to ordinary text or gameplay input.
- Check UI changes in the real screen and interaction flow. Account for narrow resolutions, scaling, long localized strings, touch, mouse, keyboard, and controller input as applicable.
- Make errors and authorization failures actionable and consistent with nearby messages. Verify both the normal and fallback user-visible states.
- Add source-language strings through the established localization path. Do not manually edit generated translation files under `data/languages`; translations go through Weblate.
- Change generated files only through their source definition or generator. Review both the generator change and generated diff, and keep generated churn out of unrelated changes.

## Verification

Verification must match the risk of the change. CI is additional coverage, not a substitute for checks that can be run locally.

### Build baseline

Use an out-of-source Ninja build. A representative Debug configuration is:

```sh
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug -Werror=dev -DDOWNLOAD_GTEST=ON -DDEV=ON
cmake --build build --target everything
cmake --build build --target run_tests
```

Build the smallest relevant target first when iteration is expensive, then build the affected product. Keep generated build artifacts outside the source tree.

### Required evidence

- Reproduce bugs before and after the fix whenever possible. Record the revision, build type, relevant configuration, platform, inputs, and exact observed result. Correct a mistaken reproduction or claim explicitly.
- Add or update a regression test when the behavior is deterministic and the repository has a suitable test layer. Exercise failure and boundary cases, not only the happy path.
- Run relevant unit tests for changed base, engine, game, protocol, map, database, or save code. For server/network/process interactions, run the integration suite against the build directory when applicable:

```sh
python scripts/integration_test.py --show-full-output build
```

Add `--test-mastersrv`, `--test-websockets`, or `--valgrind-memcheck` only when the corresponding artifacts and feature configuration are available and relevant.

- For memory safety, undefined behavior, lifetime, or integer-boundary changes, use a Debug ASan/UBSan build and run the reproducer plus relevant unit and integration tests. Use Valgrind Memcheck when it can detect the class of error or matches the workflow.
- For performance changes, provide before/after measurements under the same conditions. For optimized encoders, arithmetic, or lookup paths, use exhaustive or representative range comparisons and report mismatches, bounds, encoded sizes, and sanitizer results.
- For platform-sensitive changes, build and exercise the affected platform/configuration. Distinguish Debug from Release, client from server/headless, simulator from physical device, and compile-only evidence from runtime evidence.
- For optional code, build the relevant feature flags instead of assuming the default configuration covers it. Check both enabled and disabled configurations when interfaces or build logic change.
- Run style checks appropriate to every touched file. For C++, use clang-format 20 through `scripts/fix_style.py --dry-run` after deliberately reviewing formatting. For Python, run `ruff format --check` and `ruff check`. Run relevant documentation, spelling, shell, Rust, or generated-file checks when those files change.
- Investigate timeouts and flaky failures. Rerun only to classify them, preserve the first failure output, and never omit a failing check from the report.

Never say that a test, sanitizer, platform build, or in-game check passed unless it was actually run on the final diff. “Not run” with a reason is valid evidence; an inferred pass is not.

## Self-review before handoff

- Read the final diff from start to finish. Remove accidental formatting, debug code, stale comments, dead branches, duplicate work, and unrelated submodule or generated changes.
- Recheck every changed condition and early return for fallthrough, inverted logic, off-by-one errors, stale state, partial mutation, and inconsistent cleanup.
- Trace every changed API to all callers and implementations. Confirm headers include what they use and that optional/platform builds cannot expose a missed call site.
- Compare old and new behavior for all compatibility surfaces identified before editing. Verify that fallback and error behavior is intentional.
- Check the worktree again and list only files belonging to the task. Do not stage, commit, amend, rebase, push, or create a pull request unless the user explicitly requests it.

## Reporting and review behavior

- Lead the handoff with the observable user, player, server-operator, or developer impact. Then list the exact commands and manual scenarios run, their outcomes, and anything not tested.
- Separate verified facts from inference. Do not use generated prose, code inspection, or a successful compile as evidence for runtime behavior.
- When preparing contribution text, preserve the repository template, describe the behavior in concise human-readable words, include reproduction steps, and keep test checkboxes truthful. Disclose AI assistance where the template requests it.
- Write commit and contribution titles in the present tense and in terms of user-visible impact. Keep implementation details in the body and do not claim broader fixes than the evidence supports.
- When reviewing code, inspect the complete final change and relevant surrounding call paths. Report actionable findings with severity, exact file and line, triggering input or state, consequence, and the smallest credible fix or test. Clearly separate correctness/compatibility blockers from non-blocking suggestions.
- Do not flood a review with speculative nits. Prioritize reproducible correctness, security, compatibility, data-loss, lifetime, and concurrency problems; acknowledge uncertainty when the evidence is incomplete.
