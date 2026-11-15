#include "fyp/grading_service.hpp"

#include <algorithm>
#include <cmath>

#include "fyp/rubric.hpp"
#include "fyp/util.hpp"

namespace fyp {

std::optional<double> PhaseScores::average() const {
  if (!complete()) return std::nullopt;
  return (*supervisor + *moderator) / 2.0;
}

std::optional<double> PhaseScores::difference() const {
  if (!complete()) return std::nullopt;
  // Rounded so that scores shown as, say, 84.17 and 79.17 count as exactly 5.00 apart
  // instead of tripping the check on floating-point noise.
  return util::round2(std::fabs(*supervisor - *moderator));
}

bool PhaseScores::discrepancy() const {
  auto gap = difference();
  return gap && *gap > rubric::kDiscrepancyThreshold;
}

const char* toString(Outcome outcome) {
  switch (outcome) {
    case Outcome::Distinction: return "Distinction";
    case Outcome::Pass: return "Pass";
    case Outcome::Fail: return "Fail";
    case Outcome::Incomplete: return "Incomplete";
  }
  return "";
}

Outcome classify(const std::optional<double>& finalMark) {
  if (!finalMark) return Outcome::Incomplete;
  double mark = util::round2(*finalMark);  // classify what the user sees on screen
  if (mark >= rubric::kDistinctionMin) return Outcome::Distinction;
  if (mark >= rubric::kPassMin) return Outcome::Pass;
  return Outcome::Fail;
}

GradingService::GradingService(Database& db, Storage& storage, Clock clock)
    : Service(db, storage), clock_(std::move(clock)) {}

Status GradingService::enterMarks(const std::string& studentId, Phase phase, Role role,
                                  const std::vector<double>& marks) {
  const Student* student = db_.findStudent(studentId);
  if (!student) return Status::failure("Student " + studentId + " not found.");
  if (!db_.titleOf(studentId)) return Status::failure(student->name + " has not been confirmed to a title yet.");

  std::size_t expected = rubric::criteria(phase, role).size();
  if (marks.size() != expected) {
    return Status::failure("Expected " + std::to_string(expected) + " marks for " + toString(phase) + " " + toString(role) + ".");
  }
  for (double m : marks) {
    if (!(m >= 0.0 && m <= 100.0)) return Status::failure("Every mark must be between 0 and 100.");
  }

  if (MarkEntry* existing = db_.findMarks(studentId, phase, role)) {
    existing->marks = marks;
    existing->enteredAt = clock_();
  } else {
    db_.marks.push_back({studentId, phase, role, marks, clock_()});
  }
  commit();

  std::string message = std::string("Saved ") + toString(phase) + " " + toString(role) + " marks for " + student->name + ".";
  PhaseScores scores = phaseScores(studentId, phase);
  if (scores.discrepancy()) {
    message += "\nWARNING: supervisor (" + util::formatDouble(*scores.supervisor) + ") and moderator (" +
               util::formatDouble(*scores.moderator) + ") scores differ by " + util::formatDouble(*scores.difference()) +
               ", more than " + util::formatDouble(rubric::kDiscrepancyThreshold, 0) +
               ". The average is used, but the marks should be reviewed.";
  }
  return Status::success(message);
}

PhaseScores GradingService::phaseScores(const std::string& studentId, Phase phase) const {
  PhaseScores scores;
  if (const MarkEntry* e = db_.findMarks(studentId, phase, Role::Supervisor)) {
    scores.supervisor = rubric::score(phase, Role::Supervisor, e->marks);
  }
  if (const MarkEntry* e = db_.findMarks(studentId, phase, Role::Moderator)) {
    scores.moderator = rubric::score(phase, Role::Moderator, e->marks);
  }
  return scores;
}

std::optional<StudentResult> GradingService::resultFor(const std::string& studentId) const {
  const Student* student = db_.findStudent(studentId);
  if (!student) return std::nullopt;

  StudentResult result;
  result.student = student;
  result.title = db_.titleOf(studentId);
  result.fyp1 = phaseScores(studentId, Phase::Fyp1);
  result.fyp2 = phaseScores(studentId, Phase::Fyp2);

  auto avg1 = result.fyp1.average();
  auto avg2 = result.fyp2.average();
  if (avg1 && avg2) result.finalMark = rubric::kFyp1Weight * *avg1 + rubric::kFyp2Weight * *avg2;
  result.outcome = classify(result.finalMark);
  return result;
}

std::vector<StudentResult> GradingService::allResults() const {
  std::vector<StudentResult> results;
  for (const Student& s : db_.students) {
    if (!db_.titleOf(s.id)) continue;
    results.push_back(*resultFor(s.id));
  }
  return results;
}

Summary GradingService::summarise() const {
  Summary summary;
  double total = 0.0;
  int marked = 0;
  for (const StudentResult& r : allResults()) {
    ++summary.students;
    if (r.hasDiscrepancy()) ++summary.discrepancies;
    switch (r.outcome) {
      case Outcome::Distinction: ++summary.distinction; break;
      case Outcome::Pass: ++summary.pass; break;
      case Outcome::Fail: ++summary.fail; break;
      case Outcome::Incomplete: ++summary.incomplete; break;
    }
    if (!r.finalMark) continue;
    double mark = util::round2(*r.finalMark);
    total += mark;
    ++marked;
    summary.highest = summary.highest ? std::max(*summary.highest, mark) : mark;
    summary.lowest = summary.lowest ? std::min(*summary.lowest, mark) : mark;
  }
  if (marked > 0) summary.mean = util::round2(total / marked);
  return summary;
}

}  // namespace fyp
