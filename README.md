# FYP Management System

[![CI](https://github.com/zijing0907/fyp-manager/actions/workflows/ci.yml/badge.svg)](https://github.com/zijing0907/fyp-manager/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)
![Tests](https://img.shields.io/badge/tests-GoogleTest-4285F4)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A C++17 command-line system for managing **final-year-project (FYP) titles, student registration and grading**. Data persists in plain text files, the business logic is separated from the UI and storage, and the core is covered by automated tests.

**Tech:** C++17 · CMake · GoogleTest · GitHub Actions (Linux + macOS) · AddressSanitizer / UBSan

<p align="center">
  <img src="docs/images/demo.gif" width="720"
       alt="Demo: a lecturer opens the colour-coded class summary and a student's result, then a student browses the title list and their shortlist">
</p>

## Background

This started as a group assignment for *UECS1004/UECS1104 Programming and Problem Solving* at UTAR (June 2025): a four-person team, one 800-line source file, global arrays. I was the team coordinator. I wrote the student registration and title management features and did most of the debugging and code-style work for the rest of the team.

This repository is my redesign of that program: a layered architecture, stable identifiers, a consistent grading model, input validation, and a test suite that pins down the bugs I found in the original.

## Features

**Lecturer**

| | |
| --- | --- |
| Titles | list, search, add, **modify**, delete (with confirmation) |
| Students | register, list, confirm a student to one of their shortlisted titles |
| Grading | enter supervisor / moderator marks for FYP1 and FYP2, view a student's result, class summary |

**Student** (logs in once with their ID)

| | |
| --- | --- |
| Titles | view all titles with availability, keyword search |
| Shortlist | add / remove up to 3 titles, view shortlist and confirmed title |

Grading details:

* Supervisor and moderator scores are averaged per phase, and the final mark is 30 % FYP1 + 70 % FYP2.
* A warning is printed when the two assessors' scores differ by **more than 5 marks**, both at entry time and in reports. Raw marks are never altered.
* The class summary counts Distinction / Pass / Fail / Incomplete in a bar chart and reports mean, highest and lowest final mark.

Terminal UI:

* Sectioned menus, a status header (title availability, who is logged in, shortlist 2 / 3) and aligned tables. Long titles wrap under their own column.
* Colour carries meaning, not decoration: green is available / distinction, red is taken / fail, yellow needs attention (assessor gap, marks still missing), orange is something you type.
* Colour only appears in an interactive terminal. Piped output, files and the tests get plain ASCII. The program honours [`NO_COLOR`](https://no-color.org), and `--color` / `--no-color` override the detection. On Windows the console is switched into ANSI mode first.
* In an interactive session the program waits for Enter after each action, so a result is not scrolled away by the menu.

## Quick start

**Quickest (no CMake needed):** any C++17 compiler builds the app in one command.

```bash
c++ -std=c++17 -Iinclude src/*.cpp -o fyp_manager
cp -r sample_data data      # a working copy the program is free to edit
./fyp_manager
```

**In VS Code:** open this folder (not `src/`) and press ▶ with the Code Runner extension. The included `.vscode/settings.json` builds every source file and starts the program in the terminal.

**With CMake** (3.16+, needed for the tests; tested with Apple clang 17):

```bash
cmake -S . -B build
cmake --build build

cp -r sample_data data      # a working copy the program is free to edit
./build/fyp_manager         # or: ./build/fyp_manager --data path/to/folder
```

Colours turn on by themselves in a terminal. Add `--no-color` (or set `NO_COLOR=1`) for plain text, or `--color` to keep them when piping into a pager such as `less -R`.

Run the tests (GoogleTest is downloaded automatically at configure time; add `-DFYP_BUILD_TESTS=OFF` to skip):

```bash
ctest --test-dir build --output-on-failure
```

The sample data has 30 FYP titles, 7 fictional students and marks that exercise every outcome. Missing data files are treated as empty, so `--data some/new/folder` starts a blank system.

## Sample session

Lecturer → *Class summary*, then *Student result* for a student whose supervisor and moderator disagree:

<p align="center">
  <img src="docs/images/lecturer-report.png" width="647"
       alt="Class summary with colour-coded outcomes and a bar chart, and a student result with two assessor-gap warnings">
</p>

The same screens as plain text, which is what you get with `--no-color` or when the output is piped:

```text
--- CLASS SUMMARY  (5 students) ----------------------------------------
  ID       Name         FYP1    FYP2   Final   Outcome
  -------  ----------  -----   -----   -----   -----------
  2400001  Alice Tan   82.28   87.46   85.91   Distinction
  2400002  Bob Lim     62.28   61.98   62.07   Pass
  2400003  Chloe Wong  42.28   38.08   39.34   Fail
  2400004  Daniel Lee  71.08   74.92   73.77   Pass         ! gap
  2400005  Emily Ng    70.15       -       -   Incomplete

--- OUTCOMES -----------------------------------------------------------
  Distinction  ############              1
  Pass         ########################  2
  Fail         ############              1
  Incomplete   ############              1

  Mean 65.27   Highest 85.91   Lowest 39.34
  ! gap = supervisor/moderator scores > 5 marks apart (1 student)

--- RESULT: Daniel Lee (2400004) ---------------------------------------
  Title:  T012  Web Application Development for Person Counting

                Supervisor   Moderator     Average
  FYP1 (30%)         79.17       63.00       71.08
  FYP2 (70%)         79.07       70.77       74.92

  ! FYP1: supervisor and moderator differ by 16.17 (more than 5 marks)
  ! FYP2: supervisor and moderator differ by 8.30 (more than 5 marks)

  Final mark: 73.77  ->  Pass
```

For scripted demos, `--echo` repeats each input line so a piped session reads like a transcript:

```bash
printf '1\n11\ne\ne\n' | ./build/fyp_manager --data sample_data --echo
```

### Every screen

All of these come from one scripted session on a copy of the sample data. The lecturer adds, edits and deletes a title, registers a new student (Hannah Lim), confirms a title for Farid and enters marks for Emily. Hannah then logs in and builds her shortlist. The numbers are the menu options. Click a group to open it.

<details>
<summary><b>Lecturer</b> (13 screens)</summary>
<br>

<table>
<tr>
<td width="50%" valign="top"><b>Main menu</b><br><img src="docs/images/screens/main-menu.png" alt="Main menu"></td>
<td width="50%" valign="top"><b>Lecturer menu</b><br><img src="docs/images/screens/lecturer-menu.png" alt="Lecturer menu"></td>
</tr>
<tr>
<td valign="top"><b>1 · List all titles</b><br><img src="docs/images/screens/lecturer-list-all-titles.png" alt="List all titles"></td>
<td valign="top"><b>2 · Search titles</b><br><img src="docs/images/screens/lecturer-search-titles.png" alt="Search titles"></td>
</tr>
<tr>
<td valign="top"><b>3 · Add a title</b><br><img src="docs/images/screens/lecturer-add-a-title.png" alt="Add a title"></td>
<td valign="top"><b>4 · Modify a title</b><br><img src="docs/images/screens/lecturer-modify-a-title.png" alt="Modify a title"></td>
</tr>
<tr>
<td valign="top"><b>5 · Delete a title</b><br><img src="docs/images/screens/lecturer-delete-a-title.png" alt="Delete a title"></td>
<td valign="top"><b>6 · Register a student</b><br><img src="docs/images/screens/lecturer-register-a-student.png" alt="Register a student"></td>
</tr>
<tr>
<td valign="top"><b>7 · List students</b><br><img src="docs/images/screens/lecturer-list-students.png" alt="List students"></td>
<td valign="top"><b>8 · Confirm a student's title</b><br><img src="docs/images/screens/lecturer-confirm-a-student-title.png" alt="Confirm a student's title"></td>
</tr>
<tr>
<td valign="top"><b>9 · Enter marks</b><br><img src="docs/images/screens/lecturer-enter-marks.png" alt="Enter marks"></td>
<td valign="top"><b>10 · View a student's result</b><br><img src="docs/images/screens/lecturer-view-a-student-result.png" alt="View a student's result"></td>
</tr>
<tr>
<td valign="top"><b>11 · Class summary</b><br><img src="docs/images/screens/lecturer-class-summary.png" alt="Class summary"></td>
<td></td>
</tr>
</table>
</details>

<details>
<summary><b>Student</b> (7 screens)</summary>
<br>

<table>
<tr>
<td width="50%" valign="top"><b>Log in</b><br><img src="docs/images/screens/student-login.png" alt="Log in"></td>
<td width="50%" valign="top"><b>1 · View all titles</b><br><img src="docs/images/screens/student-view-all-titles.png" alt="View all titles"></td>
</tr>
<tr>
<td valign="top"><b>2 · Search titles</b><br><img src="docs/images/screens/student-search-titles.png" alt="Search titles"></td>
<td valign="top"><b>3 · Add to shortlist</b><br><img src="docs/images/screens/student-add-to-shortlist.png" alt="Add to shortlist"></td>
</tr>
<tr>
<td valign="top"><b>4 · Remove from shortlist</b><br><img src="docs/images/screens/student-remove-from-shortlist.png" alt="Remove from shortlist"></td>
<td valign="top"><b>5 · View my shortlist</b><br><img src="docs/images/screens/student-view-my-shortlist.png" alt="View my shortlist"></td>
</tr>
<tr>
<td valign="top"><b>A student whose title is confirmed</b><br><img src="docs/images/screens/student-confirmed.png" alt="A student whose title is confirmed"></td>
<td></td>
</tr>
</table>
</details>

## How it is built

```mermaid
flowchart LR
    cli["Cli<br/>menus and prompts"] --> svc["Services<br/>Title / Registration / Grading"]
    cli --> sty["Style<br/>colours and glyphs"]
    svc --> db[("Database<br/>in-memory structs")]
    svc --> st["Storage interface"]
    st --> fs["FileStorage<br/>text files, atomic writes"]
```

```
include/fyp/   public headers        src/    implementations        tests/   GoogleTest suite
sample_data/   starter data set      docs/   design notes, screenshots
```

* **Model** – plain structs (`Title`, `Student`, `MarkEntry`) in an in-memory `Database`. Titles have stable ids (`T001`…) so nothing breaks when another title is deleted.
* **Services** – `TitleService`, `RegistrationService`, `GradingService` hold all business rules. They return a `Status` and never print or read input.
* **Storage** – an abstract `Storage` interface with a `FileStorage` implementation: `|`-delimited text files, escaping, CRLF tolerance, per-record error recovery, and atomic write-then-rename saves.
* **CLI** – menus and prompts over injected `std::istream` / `std::ostream`, ending cleanly at end-of-input. The CLI only says what text *means* (a heading, a warning, an available title); a small `Style` decides how that looks: ANSI colour, or plain ASCII. Detecting the terminal is the only platform-specific code (`terminal.cpp`), and the services were not touched to add any of it.

The full picture, file formats and the grading formula are in [docs/design.md](docs/design.md).

## Testing

The suite covers the services, the file format (round trips, malformed lines, CRLF, crash-safety), and complete scripted CLI sessions. It also has a dedicated regression test for each defect found in the original program, for example:

* the menu loop that never terminated at end-of-input (about 156 MB of output in 3 seconds),
* confirmed titles that were forgotten on restart and then attributed to the wrong students,
* shortlists silently rewritten when an unrelated title was deleted,
* missing marks stored as `-1` and fed into the formulas.

The terminal UI has tests of its own. Plain output must be pure ASCII with no escape codes. A session run with colour, once the colour codes are stripped, must match the plain session character for character, so colour can never change what the program says. Long titles must wrap inside the layout width, and the colour decision honours `NO_COLOR`, `TERM=dumb` and the flags.

The application code compiles warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, and the suite also passes under UndefinedBehaviorSanitizer. Configure with `-DFYP_SANITIZE=ON` to add AddressSanitizer. `.github/workflows/ci.yml` builds and tests on Linux and macOS, plus a sanitizer job on Linux.

## License

[MIT](LICENSE)
