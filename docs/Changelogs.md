# Changelog

All notable changes to this project will be documented in this file. This file will be updated with every release on GitHub.

## [Unreleased]
### Added
- Initial project structure and build system (CMake).
- Basic `LogEntry` data model and `LogLevel` enumeration.
- Core `LogParser` with regex-based log parsing.
- CLI application to output log statistics summary.
- Documentation setup (`Taskflow.md`, `Decisions.md`, `Agents.md`).
- Stack trace reconstruction (`StackTraceParser`, `StackTraceBuilder`) covering
  root exceptions, `Caused by:`, `Suppressed:`, frames, and omitted-frame lines.
- Crash report parsing (`CrashReportParser`) with title, description, system
  details, mod list, and automatic input format detection (`InputDetector`).
- Exception extraction (`ExceptionExtractor`) and duplicate error grouping
  (`ErrorGrouper`) with occurrence counts, severities, and top frames.
- Rule engine (`RuleEngine`) with 56 known-issue rules (53 pattern rules, a
  repeated-error rule, a crash-report-description rule, and an
  `unrecognized-error` fallback), diagnostic confidence levels (`confirmed` /
  `likely` / `unverified`), evidence, and fix steps.
- 18 additional pattern rules covering locked files, corrupt chunks and regions,
  world save failures, malformed JSON and config files, data packs, resource
  packs, Java version mismatch, invalid JVM arguments, native JVM crashes, stack
  overflows, concurrent modification, server port binding, authentication
  failures, mod load failures, access transformers, remapping failures, and
  invalid mixin configs.
- 17 further pattern rules covering Fabric mod conflicts and entrypoint stages,
  duplicate mod versions, mixin transformer crashes, mods that reject the
  Minecraft version, worlds that reference missing or changed mods, FML
  confirmation prompts, missing data pack mods, JPMS module conflicts, malformed
  unicode escapes, ticking entities and block entities, plugin runtime errors,
  duplicate worlds, unsupported plugin API versions, and chunk downgrades.
  Message formats were cross-checked against the MIT-licensed
  [Aternos codex-minecraft](https://github.com/aternosorg/codex-minecraft)
  analyser, which powers mclo.gs insights.
- Pattern rules can inspect crash report descriptions, so description-only
  failures such as `Ticking entity` or `Ticking block entity` get a specific
  diagnosis. High-signal Forge world mod version messages are also matched when
  logged at INFO level.
- `unrecognized-error` fallback that reports honestly when no rule matched.
- Mod identification (`ModAttributor`, `ModRegistry`): diagnostics name the mod
  behind the error from crash report mod lists, mod ids written in messages,
  stack frame packages, and classes named in exception messages, and append a
  rule-aware fix step for that specific mod.
- GoogleTest suite (71 tests) with log and crash report fixtures under `tests/`.

### Changed
- Project builds as a `minesleuth_core` static library plus the `minesleuth`
  CLI executable, so the future Qt GUI can reuse the engine.
- Stack frame parsing accepts Spigot/Bukkit metadata suffixes such as
  `~[minecraft_server.jar:...]` and `[?:1.8.0_144]`, so server stack traces
  reach the diagnostic rules instead of being dropped.
