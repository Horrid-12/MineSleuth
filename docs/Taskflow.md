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
- `RuleEngine` + `DefaultRules` ship 21 rules (18 pattern rules, a repeated-error
  rule, a crash-report-description rule, and an `unrecognized-error` fallback),
  each returning severity, confidence, evidence, reasoning, and fix steps.
- `ModAttributor` names the mod behind each error: mod ids written in messages,
  stack frame packages matched against the crash report mod list, or classes
  named inside exception messages. Every diagnostic carries a `Mod:` lead and a
  fix step that names that mod (or its package) instead of generic advice.
- CLI shows the summary box, crash report context, grouped errors, and diagnostics.

## Phase 3: Desktop interface (Not Started)
- [ ] Build a custom Qt dashboard.
- [ ] Add drag-and-drop log imports.
- [ ] Create searchable log views and statistics charts.
- [ ] Add a detailed error inspection panel.

## Phase 4: Advanced features (Not Started)
- [ ] Live log monitoring.
- [ ] Compare logs from different sessions.
- [ ] Recognize common mod loader and mod-specific errors.
- [ ] Export detailed diagnostic reports.
