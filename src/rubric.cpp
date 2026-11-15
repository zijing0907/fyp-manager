#include "fyp/rubric.hpp"

#include <cassert>

namespace fyp::rubric {

namespace {

const std::vector<Criterion>& allCriteria(Phase phase) {
  static const std::vector<Criterion> fyp1 = {
      {"Report", 15, false},
      {"Presentation", 10, false},
      {"Effort", 5, true},
  };
  static const std::vector<Criterion> fyp2 = {
      {"Final Report", 20, false},
      {"Product Demo", 35, false},
      {"Presentation", 10, false},
      {"Effort", 5, true},
  };
  return phase == Phase::Fyp1 ? fyp1 : fyp2;
}

}  // namespace

std::vector<Criterion> criteria(Phase phase, Role role) {
  std::vector<Criterion> result;
  for (const Criterion& c : allCriteria(phase)) {
    if (role == Role::Supervisor || !c.supervisorOnly) result.push_back(c);
  }
  return result;
}

double score(Phase phase, Role role, const std::vector<double>& marks) {
  std::vector<Criterion> applicable = criteria(phase, role);
  assert(marks.size() == applicable.size());
  double weighted = 0.0;
  double totalWeight = 0.0;
  for (std::size_t i = 0; i < applicable.size(); ++i) {
    weighted += marks[i] * applicable[i].weight;
    totalWeight += applicable[i].weight;
  }
  return weighted / totalWeight;
}

}  // namespace fyp::rubric
