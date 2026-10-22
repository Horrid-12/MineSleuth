# MineSleuth Taskflow

This document tracks the development phases and progress of MineSleuth. It should be updated after each conversation.

## Phase 1: CLI log parser (Completed)
- [x] Read a Minecraft log file.
- [x] Parse timestamps and log levels.
- [x] Count errors, warnings, and informational messages.
- [x] Print a basic summary in the terminal.

## Phase 2: Error intelligence (Completed)
- [x] Extract Java exceptions and stack traces.
- [x] Group duplicate errors.
- [x] Create a rule-based system for known issues.
- [x] Generate basic diagnostic suggestions.

Delivered:
- `StackTraceParser` / `StackTraceBuilder` reconstruct multi-line exception blocks
  (root header, `Caused by:`, `Suppressed:`, frames, `... N more` omission lines).
- `CrashReportParser` reads `.txt` crash reports (title, description, system
  details, mod list, exception blocks); `InputDetector` chooses log vs crash report.
- `ExceptionExtractor` filters plausible exceptions; `ErrorGrouper` collapses
  repeated errors into groups with count, first line, severity, and top frame.
- `RuleEngine` + `DefaultRules` ship 56 rules (53 pattern rules, a repeated-error
  rule, a crash-report-description rule, and an `unrecognized-error` fallback),
  each returning severity, confidence, evidence, reasoning, and fix steps. The
  53 pattern rules include storage damage, locked files, config/JSON/pack
  parsing, JVM and Java version problems, port binding, authentication, mod
  loading/remapping/access-transformer/mixin failures, Fabric mod conflicts and
  entrypoints, Forge world/mod version problems, ticking entities, plugin
  runtime errors, and server world/chunk version problems. Mod-loader message
  formats were cross-checked against Aternos codex-minecraft (MIT).
- Rules also inspect crash report descriptions (for example ticking entities),
  and the stack trace parser accepts Spigot/Bukkit frame metadata suffixes so
  server exceptions still reach the rule engine.
- `ModAttributor` names the mod behind each error: mod ids written in messages,
  stack frame packages matched against the crash report mod list, or classes
  named inside exception messages. Every diagnostic carries a `Mod:` lead and a
  fix step that names that mod (or its package) instead of generic advice.
- CLI shows the summary box, crash report context, grouped errors, and diagnostics.

## Phase 3: Desktop interface (Completed)
- [x] Build a custom Qt dashboard.
- [x] Add drag-and-drop log imports.
- [x] Create searchable log views and statistics charts.
- [x] Add a detailed error inspection panel.

Delivered:
- Designed and verified minimal, user-friendly UI following Prism Launcher monotone dark theme and Futura typography.
- Implemented `MainWindow` in Qt 6 (`src/gui/main_window.h`/`.cpp`) featuring a top toolbar, session indicators, and drag-and-drop log import handlers.
- Built `DiagnosticHero` card (`src/gui/diagnostic_hero.h`/`.cpp`) presenting direct incident headlines, plain-English explanations, and prominent copyable resolution boxes.
- Created `ActionSidebar` (`src/gui/action_sidebar.h`/`.cpp`) providing quick access to `Open Mods Folder`, `Copy Crash Report`, `Upload to mclo.gs`, and `Search on Modrinth` with pure vector SVGs and zero emojis.
- Created `LogModel` (`src/gui/log_model.h`/`.cpp`) high-performance table model with `QSortFilterProxyModel` for real-time search and severity color-coding.
- Bundled `style.qss` custom Qt style sheet with the approved `#1E1E1E` canvas, `#242424` panels, and `#2675BF` Prism Blue accents.
- Configured conditional CMake GUI target `minesleuth_gui` leaving core engine and test suite fully decoupled.

## Phase 4: Advanced features (Not Started)
- [ ] Live log monitoring.
- [ ] Compare logs from different sessions.
- [ ] Recognize common mod loader and mod-specific errors.
- [ ] Export detailed diagnostic reports.
