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
- Rule engine (`RuleEngine`) with 21 known-issue rules, diagnostic confidence
  levels (`confirmed` / `likely` / `unverified`), evidence, and fix steps.
- `unrecognized-error` fallback that reports honestly when no rule matched.
- Mod identification (`ModAttributor`, `ModRegistry`): diagnostics name the mod
  behind the error from crash report mod lists, mod ids written in messages,
  stack frame packages, and classes named in exception messages, and append a
  rule-aware fix step for that specific mod.
- GoogleTest suite (65 tests) with log and crash report fixtures under `tests/`.

### Changed
- Project builds as a `minesleuth_core` static library plus the `minesleuth`
  CLI executable, so the future Qt GUI can reuse the engine.
