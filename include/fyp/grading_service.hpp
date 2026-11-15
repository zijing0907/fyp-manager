#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "fyp/service.hpp"

namespace fyp {

// Supervisor and moderator scores (each 0-100) for one phase.
struct PhaseScores {
  std::optional<double> supervisor;
  std::optional<double> moderator;

  bool complete() const { return supervisor && moderator; }
  std::optional<double> average() const;
  std::optional<double> difference() const;  // absolute gap, rounded to 2 dp
  // True when both scores exist and differ by more than rubric::kDiscrepancyThreshold.
  bool discrepancy() const;
};

enum class Outcome { Distinction, Pass, Fail, Incomplete };
const char* toString(Outcome outcome);
Outcome classify(const std::optional<double>& finalMark);

struct StudentResult {
  const Student* student = nullptr;
  const Title* title = nullptr;
  PhaseScores fyp1;
  PhaseScores fyp2;
  std::optional<double> finalMark;  // only once all four assessments are in
  Outcome outcome = Outcome::Incomplete;

  bool hasDiscrepancy() const { return fyp1.discrepancy() || fyp2.discrepancy(); }
};

struct Summary {
  int students = 0;  // students who have a confirmed title
  int distinction = 0;
  int pass = 0;
  int fail = 0;
  int incomplete = 0;
  int discrepancies = 0;  // students with at least one flagged phase
  std::optional<double> mean;
  std::optional<double> highest;
  std::optional<double> lowest;
};

// Mark entry, score calculation (averaging supervisor and moderator) and the
// results summary.
class GradingService : public Service {
 public:
  using Clock = std::function<std::string()>;

  GradingService(Database& db, Storage& storage, Clock clock);

  // Records (or replaces) one assessor's marks for one phase. `marks` follows
  // the order of rubric::criteria(phase, role). If, after this entry, the
  // supervisor and moderator scores are more than 5 apart, the success message
  // carries a warning.
  Status enterMarks(const std::string& studentId, Phase phase, Role role, const std::vector<double>& marks);

  PhaseScores phaseScores(const std::string& studentId, Phase phase) const;
  std::optional<StudentResult> resultFor(const std::string& studentId) const;
  std::vector<StudentResult> allResults() const;  // every student with a confirmed title
  Summary summarise() const;

 private:
  Clock clock_;
};

}  // namespace fyp
