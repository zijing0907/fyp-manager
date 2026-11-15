#include "fyp/cli.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "fyp/rubric.hpp"
#include "fyp/util.hpp"

namespace fyp {

Cli::Cli(Database& db, Storage& storage, std::istream& in, std::ostream& out, GradingService::Clock clock,
         Options options)
    : db_(db),
      in_(in),
      out_(out),
      options_(options),
      titles_(db, storage),
      registration_(db, storage),
      grading_(db, storage, std::move(clock)) {}

// ============================== input helpers ==============================

std::string Cli::ask(const std::string& prompt) {
  out_ << prompt << std::flush;
  std::string line;
  if (!std::getline(in_, line)) {
    out_ << '\n';
    throw InputClosed{};
  }
  line = util::trim(line);
  if (options_.echoInput) out_ << line << '\n';
  return line;
}

bool Cli::isCancel(const std::string& input) { return input == "e" || input == "E"; }

void Cli::report(const Status& status) { out_ << (status ? "" : "Error: ") << status.message << '\n'; }

const Student* Cli::askStudent() {
  while (true) {
    std::string id = ask("Student ID (7 digits, e to cancel): ");
    if (isCancel(id)) return nullptr;
    if (id.size() != 7 || !util::isAllDigits(id)) {
      out_ << "ID must be exactly 7 digits.\n";
      continue;
    }
    if (const Student* student = db_.findStudent(id)) return student;
    out_ << "Student not found.\n";
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
    out_ << "Please enter a number from 1 to " << max << ".\n";
  }
}

std::optional<double> Cli::askMark(const std::string& criterion) {
  while (true) {
    std::string input = ask("  " + criterion + " (0-100, e to cancel): ");
    if (isCancel(input)) return std::nullopt;
    double mark = 0;
    if (!util::parseDecimal(input, mark)) {
      out_ << "Please enter a number (digits and an optional decimal point).\n";
    } else if (mark > 100.0) {
      out_ << "Marks must be between 0 and 100.\n";
    } else {
      return mark;
    }
  }
}

bool Cli::confirmYes(const std::string& prompt) {
  std::string input = util::toLower(ask(prompt + " (y/n): "));
  return input == "y" || input == "yes";
}

// ============================== output helpers =============================

void Cli::printTitles(const std::vector<const Title*>& titles, bool showHolder) {
  for (const Title* t : titles) {
    out_ << t->id << "  " << t->text << "  [";
    if (!t->taken()) out_ << "Available";
    else if (showHolder) out_ << "Taken by " << t->studentId;
    else out_ << "Taken";
    out_ << "]\n";
  }
}

namespace {

std::string cell(const std::optional<double>& v) { return v ? util::formatDouble(*v) : "-"; }

std::vector<std::string> pendingAssessments(const StudentResult& r) {
  std::vector<std::string> pending;
  if (!r.fyp1.supervisor) pending.push_back("FYP1 supervisor");
  if (!r.fyp1.moderator) pending.push_back("FYP1 moderator");
  if (!r.fyp2.supervisor) pending.push_back("FYP2 supervisor");
  if (!r.fyp2.moderator) pending.push_back("FYP2 moderator");
  return pending;
}

}  // namespace

void Cli::printResult(const StudentResult& r) {
  out_ << "\n--- Result: " << r.student->name << " (" << r.student->id << ") ---\n";
  out_ << "Title: " << (r.title ? r.title->id + "  " + r.title->text : "(no title confirmed yet)") << "\n\n";

  out_ << std::left << std::setw(14) << "" << std::right << std::setw(12) << "Supervisor" << std::setw(12)
       << "Moderator" << std::setw(12) << "Average" << '\n';
  auto row = [&](const char* label, const PhaseScores& s) {
    out_ << std::left << std::setw(14) << label << std::right << std::setw(12) << cell(s.supervisor) << std::setw(12)
         << cell(s.moderator) << std::setw(12) << cell(s.average()) << '\n';
  };
  row("FYP1 (30%)", r.fyp1);
  row("FYP2 (70%)", r.fyp2);

  auto flag = [&](const char* label, const PhaseScores& s) {
    if (s.discrepancy()) {
      out_ << "! " << label << ": supervisor and moderator differ by " << util::formatDouble(*s.difference())
           << " (more than " << util::formatDouble(rubric::kDiscrepancyThreshold, 0) << " marks)\n";
    }
  };
  flag("FYP1", r.fyp1);
  flag("FYP2", r.fyp2);

  if (r.finalMark) {
    out_ << "\nFinal mark: " << util::formatDouble(*r.finalMark) << "  ->  " << toString(r.outcome) << '\n';
  } else {
    std::string waiting;
    for (const std::string& p : pendingAssessments(r)) waiting += (waiting.empty() ? "" : ", ") + p;
    out_ << "\nFinal mark: not available yet (waiting for: " << waiting << ")\n";
  }
}

// ================================= menus ===================================

void Cli::run() {
  try {
    while (true) {
      out_ << "\n====================\n      FYP System\n====================\n"
           << "1) Lecturer\n2) Student\ne) Exit\n";
      std::string choice = ask("Choice: ");
      if (choice == "1") lecturerMenu();
      else if (choice == "2") studentLogin();
      else if (isCancel(choice)) break;
      else out_ << "Invalid choice. Please try again.\n";
    }
  } catch (const InputClosed&) {
    // stdin ended: fall through to a normal shutdown
  }
  out_ << "\nGoodbye! Thank you for using the FYP System.\n";
}

void Cli::lecturerMenu() {
  while (true) {
    out_ << "\n--- Lecturer Menu ---\n"
         << "Titles:   1) List   2) Search   3) Add   4) Modify   5) Delete\n"
         << "Students: 6) Register   7) List   8) Confirm title\n"
         << "Grading:  9) Enter marks   10) Student result   11) Class summary\n"
         << "e) Back\n";
    std::string choice = ask("Choice: ");
    if (isCancel(choice)) return;
    try {
      if (choice == "1") printTitles(titles_.list(), true);
      else if (choice == "2") searchTitles();
      else if (choice == "3") addTitle();
      else if (choice == "4") modifyTitle();
      else if (choice == "5") deleteTitle();
      else if (choice == "6") registerStudent();
      else if (choice == "7") listStudents();
      else if (choice == "8") confirmTitle();
      else if (choice == "9") enterMarks();
      else if (choice == "10") showStudentResult();
      else if (choice == "11") showClassSummary();
      else out_ << "Invalid choice. Try again.\n";
    } catch (const std::runtime_error& e) {  // storage failure
      out_ << "Error: " << e.what() << "\nYour last change may not have been saved.\n";
    }
  }
}

void Cli::studentLogin() {
  const Student* student = askStudent();
  if (student) studentMenu(student->id);
}

void Cli::studentMenu(const std::string& studentId) {
  const std::string name = db_.findStudent(studentId)->name;
  while (true) {
    out_ << "\n--- Student Menu: " << name << " (" << studentId << ") ---\n"
         << "1) View titles\n2) Search titles\n3) Add to shortlist\n4) Remove from shortlist\n"
         << "5) My shortlist\ne) Log out\n";
    std::string choice = ask("Choice: ");
    if (isCancel(choice)) return;
    try {
      if (choice == "1") printTitles(titles_.list(), false);
      else if (choice == "2") searchTitles();
      else if (choice == "3") addToShortlist(studentId);
      else if (choice == "4") removeFromShortlist(studentId);
      else if (choice == "5") showShortlist(studentId);
      else out_ << "Invalid choice. Try again.\n";
    } catch (const std::runtime_error& e) {
      out_ << "Error: " << e.what() << "\nYour last change may not have been saved.\n";
    }
  }
}

// ============================= shared actions ==============================

void Cli::searchTitles() {
  std::string keywords = ask("Keywords (e to cancel): ");
  if (isCancel(keywords) || keywords.empty()) return;
  std::vector<const Title*> matches = titles_.search(keywords);
  if (matches.empty()) {
    out_ << "No titles matched.\n";
    return;
  }
  out_ << matches.size() << " match(es):\n";
  printTitles(matches, false);
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
    out_ << "Error: Title " << *id << " not found.\n";
    return;
  }
  out_ << "Current: " << title->text << '\n';
  std::string text = ask("New text (e to cancel): ");
  if (isCancel(text)) return;
  report(titles_.modify(*id, text));
}

void Cli::deleteTitle() {
  auto id = askTitleId("Title ID to delete (e to cancel): ");
  if (!id) return;
  const Title* title = db_.findTitle(*id);
  if (!title) {
    out_ << "Error: Title " << *id << " not found.\n";
    return;
  }
  if (!confirmYes("Delete \"" + title->text + "\"?")) {
    out_ << "Cancelled.\n";
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
    out_ << "No students registered.\n";
    return;
  }
  out_ << std::left << std::setw(10) << "ID" << std::setw(26) << "Name" << "Title\n";
  for (const Student& s : db_.students) {
    const Title* title = db_.titleOf(s.id);
    out_ << std::left << std::setw(10) << s.id << std::setw(26) << s.name
         << (title ? title->id : std::string("(none)")) << '\n';
  }
}

void Cli::confirmTitle() {
  const Student* student = askStudent();
  if (!student) return;
  if (const Title* existing = registration_.confirmedTitle(student->id)) {
    out_ << student->name << " is already confirmed to " << existing->id << ".\n";
    return;
  }
  std::vector<const Title*> picks = registration_.shortlist(student->id);
  if (picks.empty()) {
    out_ << student->name << " has not shortlisted any titles yet.\n";
    return;
  }
  out_ << "\nShortlist of " << student->name << ":\n";
  printTitles(picks, true);
  auto id = askTitleId("Title ID to confirm (e to cancel): ");
  if (!id) return;
  report(registration_.confirm(student->id, *id));
}

void Cli::enterMarks() {
  const Student* student = askStudent();
  if (!student) return;
  if (!db_.titleOf(student->id)) {
    out_ << student->name << " has not been confirmed to a title yet.\n";
    return;
  }
  auto roleChoice = askOption("Assessor: 1) Supervisor  2) Moderator (e to cancel): ", 2);
  if (!roleChoice) return;
  auto phaseChoice = askOption("Phase: 1) FYP1  2) FYP2 (e to cancel): ", 2);
  if (!phaseChoice) return;
  Role role = *roleChoice == 1 ? Role::Supervisor : Role::Moderator;
  Phase phase = *phaseChoice == 1 ? Phase::Fyp1 : Phase::Fyp2;

  if (const MarkEntry* existing = db_.findMarks(student->id, phase, role)) {
    out_ << toString(phase) << " " << toString(role) << " marks were already entered on " << existing->enteredAt << ".\n";
    if (!confirmYes("Replace them?")) {
      out_ << "Cancelled.\n";
      return;
    }
  }

  std::vector<double> marks;
  out_ << "Enter " << toString(phase) << " marks as " << toString(role) << ":\n";
  for (const rubric::Criterion& c : rubric::criteria(phase, role)) {
    auto mark = askMark(c.name);
    if (!mark) {
      out_ << "Cancelled. Nothing was saved.\n";
      return;
    }
    marks.push_back(*mark);
  }
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
    out_ << "No students have a confirmed title yet.\n";
    return;
  }
  out_ << "\n--- Class Summary ---\n";
  out_ << std::left << std::setw(10) << "ID" << std::setw(24) << "Name" << std::right << std::setw(9) << "FYP1"
       << std::setw(9) << "FYP2" << std::setw(9) << "Final" << "  Outcome\n";
  for (const StudentResult& r : results) {
    out_ << std::left << std::setw(10) << r.student->id << std::setw(24) << r.student->name << std::right
         << std::setw(9) << cell(r.fyp1.average()) << std::setw(9) << cell(r.fyp2.average()) << std::setw(9)
         << cell(r.finalMark) << "  " << toString(r.outcome) << (r.hasDiscrepancy() ? "  (!)" : "") << '\n';
  }

  Summary s = grading_.summarise();
  out_ << "\nStudents with a confirmed title: " << s.students << '\n'
       << "Distinction: " << s.distinction << "  |  Pass: " << s.pass << "  |  Fail: " << s.fail
       << "  |  Incomplete: " << s.incomplete << '\n';
  if (s.mean) {
    out_ << "Final mark: mean " << util::formatDouble(*s.mean) << ", highest " << util::formatDouble(*s.highest)
         << ", lowest " << util::formatDouble(*s.lowest) << '\n';
  }
  if (s.discrepancies > 0) {
    out_ << "(!) " << s.discrepancies << " student(s) with supervisor/moderator scores more than "
         << util::formatDouble(rubric::kDiscrepancyThreshold, 0) << " marks apart.\n";
  }
}

// ============================= student actions =============================

void Cli::addToShortlist(const std::string& studentId) {
  if (registration_.confirmedTitle(studentId)) {
    out_ << "You already have a confirmed title; the shortlist is closed.\n";
    return;
  }
  std::vector<const Title*> candidates;
  std::vector<const Title*> current = registration_.shortlist(studentId);
  for (const Title* t : titles_.list(true)) {
    if (std::find(current.begin(), current.end(), t) == current.end()) candidates.push_back(t);
  }
  out_ << "\nAvailable titles (you may shortlist up to " << RegistrationService::kMaxShortlist << "):\n";
  printTitles(candidates, false);
  auto id = askTitleId("Title ID to shortlist, e.g. T005 or 5 (e to cancel): ");
  if (!id) return;
  report(registration_.addToShortlist(studentId, *id));
}

void Cli::removeFromShortlist(const std::string& studentId) {
  if (registration_.shortlist(studentId).empty()) {
    out_ << "Your shortlist is empty.\n";
    return;
  }
  showShortlist(studentId);
  auto id = askTitleId("Title ID to remove (e to cancel): ");
  if (!id) return;
  report(registration_.removeFromShortlist(studentId, *id));
}

void Cli::showShortlist(const std::string& studentId) {
  out_ << "\n--- Your shortlist ---\n";
  std::vector<const Title*> picks = registration_.shortlist(studentId);
  if (picks.empty()) out_ << "(empty)\n";
  else printTitles(picks, false);
  if (const Title* confirmed = registration_.confirmedTitle(studentId)) {
    out_ << "\nConfirmed title: " << confirmed->id << "  " << confirmed->text << '\n';
  }
}

}  // namespace fyp
