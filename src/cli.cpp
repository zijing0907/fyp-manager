#include "fyp/cli.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "fyp/rubric.hpp"
#include "fyp/util.hpp"

namespace fyp {

namespace {

constexpr std::size_t kWidth = 72;       // banners, rules and wrapped text fit in an 80-column terminal
constexpr std::size_t kIndent = 2;       // left margin of everything inside a section
constexpr const char* kMargin = "  ";    // kIndent spaces
constexpr std::size_t kLabelWidth = 15;  // column where values start in "Label:  value" lines
constexpr std::size_t kBarWidth = 24;    // longest bar in the outcome chart

std::string cell(const std::optional<double>& v) { return v ? util::formatDouble(*v) : "-"; }

std::string plural(std::size_t n, const std::string& one, const std::string& many) {
  return std::to_string(n) + " " + (n == 1 ? one : many);
}

std::vector<std::string> pendingAssessments(const StudentResult& r) {
  std::vector<std::string> pending;
  if (!r.fyp1.supervisor) pending.push_back("FYP1 supervisor");
  if (!r.fyp1.moderator) pending.push_back("FYP1 moderator");
  if (!r.fyp2.supervisor) pending.push_back("FYP2 supervisor");
  if (!r.fyp2.moderator) pending.push_back("FYP2 moderator");
  return pending;
}

Tone toneOf(Outcome outcome) {
  switch (outcome) {
    case Outcome::Distinction: return Tone::Good;
    case Outcome::Pass: return Tone::Info;
    case Outcome::Fail: return Tone::Bad;
    case Outcome::Incomplete: return Tone::Muted;
  }
  return Tone::Plain;
}

std::string phaseLabel(Phase phase) {
  double weight = phase == Phase::Fyp1 ? rubric::kFyp1Weight : rubric::kFyp2Weight;
  return std::string(toString(phase)) + " (" + util::formatDouble(weight * 100.0, 0) + "%)";
}

}  // namespace

Cli::Cli(Database& db, Storage& storage, std::istream& in, std::ostream& out, GradingService::Clock clock,
         Options options)
    : db_(db),
      in_(in),
      out_(out),
      options_(options),
      style_(options.style),
      titles_(db, storage),
      registration_(db, storage),
      grading_(db, storage, std::move(clock)) {}

// ============================== input helpers ==============================

std::string Cli::ask(const std::string& prompt) {
  out_ << kMargin << style_.paint(prompt, Tone::Key) << std::flush;
  std::string line;
  if (!std::getline(in_, line)) {
    out_ << '\n';
    throw InputClosed{};
  }
  line = util::trim(line);
  if (options_.echoInput) out_ << style_.paint(line, Tone::Strong) << '\n';
  return line;
}

bool Cli::isCancel(const std::string& input) { return input == "e" || input == "E"; }

void Cli::report(const Status& status) {
  if (!status) {
    say("Error: " + status.message, Tone::Bad);
    return;
  }
  for (const std::string& line : util::splitList(status.message, '\n')) {
    say(line, line.rfind("WARNING", 0) == 0 ? Tone::Warn : Tone::Good);
  }
}

const Student* Cli::askStudent() {
  while (true) {
    std::string id = ask("Student ID (7 digits, e to cancel): ");
    if (isCancel(id)) return nullptr;
    if (id.size() != 7 || !util::isAllDigits(id)) {
      say("ID must be exactly 7 digits.", Tone::Warn);
      continue;
    }
    if (const Student* student = db_.findStudent(id)) return student;
    say("Student not found.", Tone::Warn);
    return nullptr;
  }
}

std::optional<std::string> Cli::askTitleId(const std::string& prompt) {
  std::string input = ask(prompt);
  if (isCancel(input) || input.empty()) return std::nullopt;
  return normaliseTitleId(input);
}

std::optional<int> Cli::askOption(const std::string& prompt, int max) {
  while (true) {
    std::string input = ask(prompt);
    if (isCancel(input)) return std::nullopt;
    if (input.size() == 1 && input[0] >= '1' && input[0] < '1' + max) return input[0] - '0';
    say("Please enter a number from 1 to " + std::to_string(max) + ".", Tone::Warn);
  }
}

std::optional<double> Cli::askMark(const std::string& criterion) {
  while (true) {
    std::string input = ask(criterion + " (0-100, e to cancel): ");
    if (isCancel(input)) return std::nullopt;
    double mark = 0;
    if (!util::parseDecimal(input, mark)) {
      say("Please enter a number (digits and an optional decimal point).", Tone::Warn);
    } else if (mark > 100.0) {
      say("Marks must be between 0 and 100.", Tone::Warn);
    } else {
      return mark;
    }
  }
}

void Cli::pause() {
  if (!options_.pauseAfterActions) return;
  out_ << '\n';
  ask("Press Enter to return to the menu...");
}

bool Cli::confirmYes(const std::string& prompt) {
  std::string input = util::toLower(ask(prompt + " (y/n): "));
  return input == "y" || input == "yes";
}

// ============================== layout helpers =============================

void Cli::say(const std::string& text, Tone tone) { out_ << kMargin << style_.paint(text, tone) << '\n'; }

void Cli::printBanner(const std::string& title, const std::string& subtitle) {
  const std::size_t inner = kWidth - 2;
  const std::string rule = style_.paint("+" + std::string(inner, '=') + "+", Tone::Label);
  const std::string side = style_.paint("|", Tone::Label);
  auto centred = [&](const std::string& text, Tone tone) {
    std::size_t w = util::displayWidth(text);
    std::size_t left = w < inner ? (inner - w) / 2 : 0;
    std::size_t right = w < inner ? inner - w - left : 0;
    out_ << side << std::string(left, ' ') << style_.paint(text, tone) << std::string(right, ' ') << side << '\n';
  };
  out_ << '\n' << rule << '\n';
  centred(title, Tone::Heading);
  if (!subtitle.empty()) centred(subtitle, Tone::Muted);
  out_ << rule << '\n';
}

void Cli::printSection(const std::string& name) {
  const std::size_t used = 4 + util::displayWidth(name) + 1;  // "--- " + name + " "
  out_ << '\n'
       << style_.paint("--- ", Tone::Label) << style_.paint(name, Tone::Heading) << ' '
       << style_.paint(std::string(used < kWidth ? kWidth - used : 3, '-'), Tone::Label) << '\n';
}

void Cli::printMenu(const std::vector<MenuSection>& sections, const std::string& exitLabel) {
  std::size_t keyWidth = 3;  // "[e]"
  for (const MenuSection& section : sections) {
    for (const MenuItem& item : section.items) keyWidth = std::max(keyWidth, item.key.size() + 2);
  }
  auto line = [&](const std::string& key, const std::string& label) {
    const std::string tag = "[" + key + "]";
    out_ << kMargin << style_.paint(tag, Tone::Key) << std::string(keyWidth - tag.size() + 1, ' ') << label << '\n';
  };
  for (const MenuSection& section : sections) {
    printSection(section.name);
    for (const MenuItem& item : section.items) line(item.key, item.label);
  }
  out_ << '\n';
  line("e", exitLabel);
  out_ << '\n';
}

void Cli::printField(const std::string& label, const std::string& value, Tone tone, const std::string& note) {
  const std::size_t w = util::displayWidth(label);
  out_ << kMargin << style_.paint(label, Tone::Label) << std::string(w < kLabelWidth ? kLabelWidth - w : 1, ' ')
       << style_.paint(value, tone);
  if (!note.empty()) out_ << "  " << style_.paint(note, Tone::Muted);
  out_ << '\n';
}

void Cli::printConfirmedTitle(const Title& title) {
  printField("Confirmed:", title.id, Tone::Good);
  const std::size_t column = kIndent + kLabelWidth;
  for (const std::string& line : util::wrapText(title.text, kWidth - column)) {
    out_ << std::string(column, ' ') << line << '\n';
  }
}

// ============================== output helpers =============================

void Cli::printTitles(const std::vector<const Title*>& titles, bool showHolder) {
  if (titles.empty()) {
    say("(none)", Tone::Muted);
    return;
  }
  std::size_t idWidth = 4;
  for (const Title* t : titles) idWidth = std::max(idWidth, util::displayWidth(t->id));
  const std::size_t statusWidth = 9;  // "Available"
  const std::size_t column = kIndent + idWidth + 2 + statusWidth + 2;
  const std::string hang(column, ' ');

  for (const Title* t : titles) {
    const bool taken = t->taken();
    const Tone textTone = taken ? Tone::Muted : Tone::Plain;
    std::vector<std::string> lines = util::wrapText(t->text, kWidth - column);
    out_ << kMargin << util::padRight(t->id, idWidth) << "  "
         << style_.paint(util::padRight(taken ? "Taken" : "Available", statusWidth), taken ? Tone::Bad : Tone::Good)
         << "  " << style_.paint(lines[0], textTone) << '\n';
    for (std::size_t i = 1; i < lines.size(); ++i) out_ << hang << style_.paint(lines[i], textTone) << '\n';
    if (taken && showHolder) {
      const Student* holder = db_.findStudent(t->studentId);
      out_ << hang << style_.paint("held by ", Tone::Muted)
           << (holder ? holder->name + " (" + holder->id + ")" : t->studentId) << '\n';
    }
  }
}

void Cli::printResult(const StudentResult& r) {
  printSection("RESULT: " + r.student->name + " (" + r.student->id + ")");
  if (r.title) {
    const std::size_t column = kIndent + 8 + util::displayWidth(r.title->id) + 2;  // "Title:  " + id + "  "
    std::vector<std::string> lines = util::wrapText(r.title->text, kWidth - column);
    out_ << kMargin << style_.paint("Title:", Tone::Label) << "  " << style_.paint(r.title->id, Tone::Muted) << "  "
         << style_.paint(lines[0], Tone::Strong) << '\n';
    for (std::size_t i = 1; i < lines.size(); ++i) {
      out_ << std::string(column, ' ') << style_.paint(lines[i], Tone::Strong) << '\n';
    }
  } else {
    out_ << kMargin << style_.paint("Title:", Tone::Label) << "  " << style_.paint("(no title confirmed yet)", Tone::Muted)
         << '\n';
  }

  out_ << '\n'
       << kMargin
       << style_.paint(std::string(12, ' ') + util::padLeft("Supervisor", 12) + util::padLeft("Moderator", 12) +
                           util::padLeft("Average", 12),
                       Tone::Strong)
       << '\n';
  auto row = [&](Phase phase, const PhaseScores& s) {
    out_ << kMargin << util::padRight(phaseLabel(phase), 12) << util::padLeft(cell(s.supervisor), 12)
         << util::padLeft(cell(s.moderator), 12) << style_.paint(util::padLeft(cell(s.average()), 12), Tone::Strong)
         << '\n';
  };
  row(Phase::Fyp1, r.fyp1);
  row(Phase::Fyp2, r.fyp2);

  if (r.hasDiscrepancy()) out_ << '\n';
  auto flag = [&](Phase phase, const PhaseScores& s) {
    if (s.discrepancy()) {
      say(std::string("! ") + toString(phase) + ": supervisor and moderator differ by " +
              util::formatDouble(*s.difference()) + " (more than " +
              util::formatDouble(rubric::kDiscrepancyThreshold, 0) + " marks)",
          Tone::Warn);
    }
  };
  flag(Phase::Fyp1, r.fyp1);
  flag(Phase::Fyp2, r.fyp2);

  out_ << '\n' << kMargin << style_.paint("Final mark:", Tone::Label) << ' ';
  if (r.finalMark) {
    out_ << style_.paint(util::formatDouble(*r.finalMark), Tone::Strong) << "  " << style_.arrow() << ' '
         << style_.badge(toString(r.outcome), toneOf(r.outcome)) << '\n';
  } else {
    std::string waiting;
    for (const std::string& p : pendingAssessments(r)) waiting += (waiting.empty() ? "" : ", ") + p;
    out_ << style_.paint("not available yet", Tone::Warn) << ' '
         << style_.paint("(waiting for: " + waiting + ")", Tone::Muted) << '\n';
  }
}

// ================================= menus ===================================

void Cli::run() {
  static const std::vector<MenuSection> kMenu = {
      {"SELECT ROLE", {{"1", "Lecturer"}, {"2", "Student"}}},
  };
  try {
    while (true) {
      const std::string dot = style_.dot();
      printBanner("FYP MANAGEMENT SYSTEM", "Titles  " + dot + "  Registration  " + dot + "  Grading");

      const std::size_t available = titles_.list(true).size();
      const std::size_t confirmed = db_.titles.size() - available;  // a taken title has exactly one student
      const std::string titleCount = std::to_string(db_.titles.size());
      const std::string studentCount = std::to_string(db_.students.size());
      const std::size_t valueWidth = std::max(titleCount.size(), studentCount.size());
      out_ << '\n';
      printField("Titles:", util::padRight(titleCount, valueWidth), Tone::Strong,
                 "(" + std::to_string(available) + " available)");
      printField("Students:", util::padRight(studentCount, valueWidth), Tone::Strong,
                 "(" + std::to_string(confirmed) + " with a confirmed title)");

      printMenu(kMenu, "Exit");
      std::string choice = ask("Select option (1-2, e): ");
      if (choice == "1") lecturerMenu();
      else if (choice == "2") studentLogin();
      else if (isCancel(choice)) break;
      else say("Invalid choice. Please try again.", Tone::Warn);
    }
  } catch (const InputClosed&) {
    // stdin ended: fall through to a normal shutdown
  }
  out_ << '\n';
  say("Goodbye! Thank you for using the FYP System.", Tone::Label);
}

void Cli::lecturerMenu() {
  static const std::vector<MenuSection> kMenu = {
      {"PROJECT TITLES",
       {{"1", "List all titles"}, {"2", "Search titles"}, {"3", "Add a title"}, {"4", "Modify a title"},
        {"5", "Delete a title"}}},
      {"STUDENTS", {{"6", "Register a student"}, {"7", "List students"}, {"8", "Confirm a student's title"}}},
      {"GRADING", {{"9", "Enter marks"}, {"10", "View a student's result"}, {"11", "Class summary"}}},
  };
  while (true) {
    printBanner("FYP MANAGER  -  LECTURER PANEL");
    printMenu(kMenu, "Back");
    std::string choice = ask("Select option (1-11, e): ");
    if (isCancel(choice)) return;
    try {
      if (choice == "1") listTitles(true);
      else if (choice == "2") searchTitles(true);
      else if (choice == "3") addTitle();
      else if (choice == "4") modifyTitle();
      else if (choice == "5") deleteTitle();
      else if (choice == "6") registerStudent();
      else if (choice == "7") listStudents();
      else if (choice == "8") confirmTitle();
      else if (choice == "9") enterMarks();
      else if (choice == "10") showStudentResult();
      else if (choice == "11") showClassSummary();
      else {
        say("Invalid choice. Try again.", Tone::Warn);
        continue;
      }
    } catch (const std::runtime_error& e) {  // storage failure
      say(std::string("Error: ") + e.what(), Tone::Bad);
      say("Your last change may not have been saved.", Tone::Bad);
    }
    pause();
  }
}

void Cli::studentLogin() {
  const Student* student = askStudent();
  if (student) studentMenu(student->id);
}

void Cli::studentMenu(const std::string& studentId) {
  static const std::vector<MenuSection> kMenu = {
      {"PROJECT TITLES", {{"1", "View all titles"}, {"2", "Search titles"}}},
      {"MY SHORTLIST", {{"3", "Add to shortlist"}, {"4", "Remove from shortlist"}, {"5", "View my shortlist"}}},
  };
  const std::string name = db_.findStudent(studentId)->name;
  while (true) {
    printBanner("FYP MANAGER  -  STUDENT PORTAL");
    out_ << '\n';
    printField("Logged in as:", name + " (" + studentId + ")", Tone::Strong);
    if (const Title* confirmed = registration_.confirmedTitle(studentId)) {
      printConfirmedTitle(*confirmed);
    } else {
      const std::size_t picks = registration_.shortlist(studentId).size();
      const std::size_t max = RegistrationService::kMaxShortlist;
      printField("Shortlist:", std::to_string(picks) + " / " + std::to_string(max), Tone::Strong,
                 picks >= max ? "(full)" : "");
      printField("Confirmed:", "not yet", Tone::Warn, "(a lecturer will confirm one)");
    }
    printMenu(kMenu, "Log out");
    std::string choice = ask("Select option (1-5, e): ");
    if (isCancel(choice)) return;
    try {
      if (choice == "1") listTitles(false);
      else if (choice == "2") searchTitles(false);
      else if (choice == "3") addToShortlist(studentId);
      else if (choice == "4") removeFromShortlist(studentId);
      else if (choice == "5") showShortlist(studentId);
      else {
        say("Invalid choice. Try again.", Tone::Warn);
        continue;
      }
    } catch (const std::runtime_error& e) {
      say(std::string("Error: ") + e.what(), Tone::Bad);
      say("Your last change may not have been saved.", Tone::Bad);
    }
    pause();
  }
}

// ============================= shared actions ==============================

void Cli::listTitles(bool showHolder) {
  std::vector<const Title*> all = titles_.list();
  const std::size_t available = titles_.list(true).size();
  printSection("ALL TITLES  (" + std::to_string(available) + " of " + std::to_string(all.size()) + " available)");
  printTitles(all, showHolder);
}

void Cli::searchTitles(bool showHolder) {
  std::string keywords = ask("Keywords (e to cancel): ");
  if (isCancel(keywords) || keywords.empty()) return;
  std::vector<const Title*> matches = titles_.search(keywords);
  printSection("RESULTS FOR \"" + keywords + "\"  (" + plural(matches.size(), "match", "matches") + ")");
  if (matches.empty()) {
    say("No titles matched.", Tone::Muted);
    return;
  }
  printTitles(matches, showHolder);
}

// ============================ lecturer actions =============================

void Cli::addTitle() {
  std::string text = ask("New title (e to cancel): ");
  if (isCancel(text)) return;
  report(titles_.add(text));
}

void Cli::modifyTitle() {
  auto id = askTitleId("Title ID to modify, e.g. T005 or 5 (e to cancel): ");
  if (!id) return;
  const Title* title = db_.findTitle(*id);
  if (!title) {
    say("Error: Title " + *id + " not found.", Tone::Bad);
    return;
  }
  out_ << kMargin << style_.paint("Current:", Tone::Label) << ' ' << title->text << '\n';
  std::string text = ask("New text (e to cancel): ");
  if (isCancel(text)) return;
  report(titles_.modify(*id, text));
}

void Cli::deleteTitle() {
  auto id = askTitleId("Title ID to delete (e to cancel): ");
  if (!id) return;
  const Title* title = db_.findTitle(*id);
  if (!title) {
    say("Error: Title " + *id + " not found.", Tone::Bad);
    return;
  }
  if (!confirmYes("Delete \"" + title->text + "\"?")) {
    say("Cancelled.", Tone::Muted);
    return;
  }
  report(titles_.remove(*id));
}

void Cli::registerStudent() {
  while (true) {
    std::string id = ask("New student ID (7 digits, e to cancel): ");
    if (isCancel(id)) return;
    std::string name = ask("Student name (e to cancel): ");
    if (isCancel(name)) return;
    Status status = registration_.registerStudent(id, name);
    report(status);
    if (status) return;
  }
}

void Cli::listStudents() {
  if (db_.students.empty()) {
    say("No students registered.", Tone::Muted);
    return;
  }
  std::size_t nameWidth = 4;
  for (const Student& s : db_.students) nameWidth = std::max(nameWidth, util::displayWidth(s.name));
  const std::string max = std::to_string(RegistrationService::kMaxShortlist);

  printSection("STUDENTS  (" + std::to_string(db_.students.size()) + ")");
  out_ << kMargin
       << style_.paint(util::padRight("ID", 9) + util::padRight("Name", nameWidth + 2) + util::padRight("Shortlist", 11) +
                           "Title",
                       Tone::Strong)
       << '\n';
  out_ << kMargin
       << style_.paint(std::string(7, '-') + "  " + std::string(nameWidth, '-') + "  " + std::string(9, '-') + "  " +
                           std::string(5, '-'),
                       Tone::Muted)
       << '\n';
  for (const Student& s : db_.students) {
    const Title* title = db_.titleOf(s.id);
    const std::string picks = std::to_string(registration_.shortlist(s.id).size()) + " / " + max;
    out_ << kMargin << util::padRight(s.id, 9) << util::padRight(s.name, nameWidth + 2) << util::padRight(picks, 11)
         << (title ? style_.paint(title->id, Tone::Good) : style_.paint("-", Tone::Muted)) << '\n';
  }
}

void Cli::confirmTitle() {
  const Student* student = askStudent();
  if (!student) return;
  if (const Title* existing = registration_.confirmedTitle(student->id)) {
    say(student->name + " is already confirmed to " + existing->id + ".", Tone::Warn);
    return;
  }
  std::vector<const Title*> picks = registration_.shortlist(student->id);
  if (picks.empty()) {
    say(student->name + " has not shortlisted any titles yet.", Tone::Warn);
    return;
  }
  printSection("SHORTLIST: " + student->name + " (" + student->id + ")");
  printTitles(picks, true);
  out_ << '\n';
  auto id = askTitleId("Title ID to confirm (e to cancel): ");
  if (!id) return;
  report(registration_.confirm(student->id, *id));
}

void Cli::enterMarks() {
  const Student* student = askStudent();
  if (!student) return;
  if (!db_.titleOf(student->id)) {
    say(student->name + " has not been confirmed to a title yet.", Tone::Warn);
    return;
  }
  auto roleChoice = askOption("Assessor  [1] Supervisor  [2] Moderator  (e to cancel): ", 2);
  if (!roleChoice) return;
  auto phaseChoice = askOption("Phase  [1] FYP1  [2] FYP2  (e to cancel): ", 2);
  if (!phaseChoice) return;
  Role role = *roleChoice == 1 ? Role::Supervisor : Role::Moderator;
  Phase phase = *phaseChoice == 1 ? Phase::Fyp1 : Phase::Fyp2;

  if (const MarkEntry* existing = db_.findMarks(student->id, phase, role)) {
    say(std::string(toString(phase)) + " " + toString(role) + " marks were already entered on " + existing->enteredAt +
            ".",
        Tone::Warn);
    if (!confirmYes("Replace them?")) {
      say("Cancelled.", Tone::Muted);
      return;
    }
  }

  printSection(std::string(toString(phase)) + " " + toString(role) + " marks: " + student->name);
  const std::vector<rubric::Criterion> criteria = rubric::criteria(phase, role);
  std::size_t nameWidth = 0;
  for (const rubric::Criterion& c : criteria) nameWidth = std::max(nameWidth, util::displayWidth(c.name));

  std::vector<double> marks;
  for (const rubric::Criterion& c : criteria) {
    auto mark = askMark(util::padRight(c.name, nameWidth));
    if (!mark) {
      say("Cancelled. Nothing was saved.", Tone::Muted);
      return;
    }
    marks.push_back(*mark);
  }
  out_ << '\n';
  report(grading_.enterMarks(student->id, phase, role, marks));
}

void Cli::showStudentResult() {
  const Student* student = askStudent();
  if (!student) return;
  printResult(*grading_.resultFor(student->id));
}

void Cli::showClassSummary() {
  std::vector<StudentResult> results = grading_.allResults();
  if (results.empty()) {
    say("No students have a confirmed title yet.", Tone::Muted);
    return;
  }
  const Summary s = grading_.summarise();
  std::size_t nameWidth = 4;
  for (const StudentResult& r : results) nameWidth = std::max(nameWidth, util::displayWidth(r.student->name));

  printSection("CLASS SUMMARY  (" + plural(static_cast<std::size_t>(s.students), "student", "students") + ")");
  out_ << kMargin
       << style_.paint(util::padRight("ID", 9) + util::padRight("Name", nameWidth + 2) + util::padLeft("FYP1", 5) +
                           util::padLeft("FYP2", 8) + util::padLeft("Final", 8) + "   Outcome",
                       Tone::Strong)
       << '\n';
  out_ << kMargin
       << style_.paint(std::string(7, '-') + "  " + std::string(nameWidth, '-') + "  " + std::string(5, '-') + "   " +
                           std::string(5, '-') + "   " + std::string(5, '-') + "   " + std::string(11, '-'),
                       Tone::Muted)
       << '\n';
  for (const StudentResult& r : results) {
    const std::string outcome = toString(r.outcome);
    out_ << kMargin << util::padRight(r.student->id, 9) << util::padRight(r.student->name, nameWidth + 2)
         << util::padLeft(cell(r.fyp1.average()), 5) << util::padLeft(cell(r.fyp2.average()), 8)
         << style_.paint(util::padLeft(cell(r.finalMark), 8), Tone::Strong) << "   ";
    if (r.hasDiscrepancy()) {
      out_ << style_.paint(util::padRight(outcome, 11), toneOf(r.outcome)) << "  " << style_.paint("! gap", Tone::Warn);
    } else {
      out_ << style_.paint(outcome, toneOf(r.outcome));
    }
    out_ << '\n';
  }

  printSection("OUTCOMES");
  const std::pair<Outcome, int> counts[] = {
      {Outcome::Distinction, s.distinction}, {Outcome::Pass, s.pass}, {Outcome::Fail, s.fail}, {Outcome::Incomplete, s.incomplete}};
  int most = 1;
  for (const auto& [outcome, n] : counts) most = std::max(most, n);
  for (const auto& [outcome, n] : counts) {
    std::size_t length = 0;
    if (n > 0) {
      double scaled = static_cast<double>(n) * static_cast<double>(kBarWidth) / static_cast<double>(most);
      length = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(scaled)));
    }
    out_ << kMargin << util::padRight(toString(outcome), 13)
         << style_.paint(util::repeat(style_.barBlock(), length), toneOf(outcome)) << std::string(kBarWidth - length, ' ')
         << "  " << style_.paint(std::to_string(n), Tone::Strong) << '\n';
  }

  if (s.mean) {
    out_ << '\n'
         << kMargin << style_.paint("Mean", Tone::Label) << ' ' << style_.paint(util::formatDouble(*s.mean), Tone::Strong)
         << "   " << style_.paint("Highest", Tone::Label) << ' '
         << style_.paint(util::formatDouble(*s.highest), Tone::Strong) << "   " << style_.paint("Lowest", Tone::Label)
         << ' ' << style_.paint(util::formatDouble(*s.lowest), Tone::Strong) << '\n';
  }
  if (s.discrepancies > 0) {
    say("! gap = supervisor/moderator scores > " + util::formatDouble(rubric::kDiscrepancyThreshold, 0) +
            " marks apart (" + plural(static_cast<std::size_t>(s.discrepancies), "student", "students") + ")",
        Tone::Warn);
  }
}

// ============================= student actions =============================

void Cli::addToShortlist(const std::string& studentId) {
  if (registration_.confirmedTitle(studentId)) {
    say("You already have a confirmed title; the shortlist is closed.", Tone::Warn);
    return;
  }
  std::vector<const Title*> candidates;
  std::vector<const Title*> current = registration_.shortlist(studentId);
  for (const Title* t : titles_.list(true)) {
    if (std::find(current.begin(), current.end(), t) == current.end()) candidates.push_back(t);
  }
  printSection("AVAILABLE TITLES");
  say("You can shortlist up to " + std::to_string(RegistrationService::kMaxShortlist) + " titles (" +
          std::to_string(current.size()) + " chosen so far).",
      Tone::Muted);
  out_ << '\n';
  printTitles(candidates, false);
  out_ << '\n';
  auto id = askTitleId("Title ID to shortlist, e.g. T005 or 5 (e to cancel): ");
  if (!id) return;
  report(registration_.addToShortlist(studentId, *id));
}

void Cli::removeFromShortlist(const std::string& studentId) {
  if (registration_.shortlist(studentId).empty()) {
    say("Your shortlist is empty.", Tone::Muted);
    return;
  }
  showShortlist(studentId);
  out_ << '\n';
  auto id = askTitleId("Title ID to remove (e to cancel): ");
  if (!id) return;
  report(registration_.removeFromShortlist(studentId, *id));
}

void Cli::showShortlist(const std::string& studentId) {
  std::vector<const Title*> picks = registration_.shortlist(studentId);
  printSection("MY SHORTLIST  (" + std::to_string(picks.size()) + " of " +
               std::to_string(RegistrationService::kMaxShortlist) + ")");
  if (picks.empty()) say("(empty)", Tone::Muted);
  else printTitles(picks, false);
  if (const Title* confirmed = registration_.confirmedTitle(studentId)) {
    out_ << '\n';
    printConfirmedTitle(*confirmed);
  }
}

}  // namespace fyp
