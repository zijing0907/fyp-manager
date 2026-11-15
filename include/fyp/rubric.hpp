#pragma once
// The marking scheme, kept in one place so it is easy to read and to change.
//
// Each assessor marks every applicable criterion out of 100. Their weighted
// total is then normalised back to a 0-100 scale using only the criteria that
// role actually marks. That makes supervisor and moderator scores directly
// comparable, which is what the ">5 marks apart" check needs. (Moderators do
// not mark "Effort", so their raw weights sum to less than the supervisor's.)

#include <string>
#include <vector>

#include "fyp/models.hpp"

namespace fyp::rubric {

struct Criterion {
  std::string name;
  double weight;         // relative weight within the phase
  bool supervisorOnly;   // moderators do not mark this criterion
};

constexpr double kFyp1Weight = 0.30;  // share of the final mark
constexpr double kFyp2Weight = 0.70;
constexpr double kDiscrepancyThreshold = 5.0;  // supervisor/moderator gap that triggers a warning
constexpr double kDistinctionMin = 75.0;
constexpr double kPassMin = 50.0;

// Criteria this role marks in this phase, in the order marks are entered.
std::vector<Criterion> criteria(Phase phase, Role role);

// Weighted, normalised 0-100 score. `marks` must hold one value per criterion
// returned by criteria(phase, role).
double score(Phase phase, Role role, const std::vector<double>& marks);

}  // namespace fyp::rubric
