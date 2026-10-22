<div align="center">
  <h1>MineSleuth</h1>
  <p>A fast, lightweight Minecraft log analyzer (currently in early development) designed to detect errors and identify crash causes.</p>

  <p>
    <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&amp;logo=c%2B%2B&amp;logoColor=white" alt="C++20">
    <img src="https://img.shields.io/badge/CMake-3.15%2B-064F8C?style=flat&amp;logo=cmake&amp;logoColor=white" alt="CMake">
    <img src="https://img.shields.io/badge/Qt-6-41CD52?style=flat&amp;logo=qt&amp;logoColor=white" alt="Qt 6">
    <img src="https://img.shields.io/badge/License-MIT-green?style=flat" alt="License">
    <img src="https://img.shields.io/badge/Status-Early_Development-orange?style=flat" alt="Status">
    <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac%20%7C%20Linux-lightgrey?style=flat" alt="Platforms">
  </p>
</div>

<p align="center">
  <a href="#current-features">Current Features</a> ·
  <a href="#roadmap">Roadmap</a> ·
  <a href="#architecture">Architecture</a> ·
  <a href="#get-started">Get Started</a> ·
  <a href="#installation">Installation</a> ·
  <a href="#tech-stack">Tech Stack</a>
</p>

## Current Features

### CLI Log Parser
- Parses `latest.log` and standard Minecraft server logs.
- Extracts timestamps, log levels (INFO, WARN, ERROR, FATAL, DEBUG, TRACE), threads, and message bodies.
- Utilizes `std::regex` to process standard formats like `[HH:MM:SS] [Thread/Level]: Message`.
- Prints a fast ASCII summary dashboard in the terminal detailing total entries and categorized error counts.

### Error Intelligence
- Reconstructs multiline Java exceptions: root header, `Caused by:`, `Suppressed:`, stack frames, and omitted-frame markers.
- Reads Minecraft `.txt` crash reports (description, system details, mod list) and auto-detects the input format.
- Groups duplicate errors so one issue appears once with its occurrence count and top frame.
- Matches 56 known-issue rules and reports each finding with severity, confidence, evidence, and fix steps.
- Names the specific mod behind each error (crash report mod list, mod ids in messages, stack frame packages) and gives a mod-specific fix step instead of generic advice.
- Cross-checks mod-loader message formats against the MIT-licensed [Aternos codex-minecraft](https://github.com/aternosorg/codex-minecraft) analyser, the engine behind mclo.gs insights.
- Falls back to an explicit `unrecognized-error` diagnosis instead of guessing.
- Ships a GoogleTest suite (71 tests) with log and crash report fixtures.

### Desktop Interface
- A responsive Qt 6 graphical dashboard with a Monotonic Prism Launcher Inspired UI.
- Implement drag-and-drop log imports and searchable log views.
- Add detailed diagnostic inspection panels and interactive statistic charts.

## Roadmap

### Phase 4: Advanced Features
- Live log monitoring and file watching.
- Advanced Mod Dependency graphs and conflict visualization.
- Export detailed diagnostic HTML/JSON reports.

## Architecture

MineSleuth uses a modular architecture, strictly separating the core C++20 parsing engine from the future Qt 6 GUI framework.


## Get Started

### Clone and Build

<details>
<summary>Windows (PowerShell)</summary>

```powershell
git clone https://github.com/yourusername/MineSleuth.git
cd MineSleuth
cmake -B build -S .
cmake --build build
./build/src/minesleuth.exe path/to/latest.log
```
</details>

<details>
<summary>Linux / macOS</summary>

```sh
git clone https://github.com/yourusername/MineSleuth.git
cd MineSleuth
cmake -B build -S .
cmake --build build
./build/src/minesleuth path/to/latest.log
```
</details>

### Run the Tests

<details>
<summary>All platforms</summary>

```sh
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```
</details>

## Installation

| Platform | Status |
|---|---|
| Windows | Supported (Build from source) |
| macOS | Supported (Build from source) |
| Linux | Supported (Build from source) |

## Tech Stack

| Layer | Stack |
|---|---|
| Language | ![](https://img.shields.io/badge/C%2B%2B20-00599C?style=flat&amp;logo=c%2B%2B&amp;logoColor=white) |
| Build System | ![](https://img.shields.io/badge/CMake-064F8C?style=flat&amp;logo=cmake&amp;logoColor=white) |
| GUI Framework | ![](https://img.shields.io/badge/Qt_6-41CD52?style=flat&amp;logo=qt&amp;logoColor=white) |
