#pragma once

#include <string>
#include <vector>

#include "fyp/service.hpp"

namespace fyp {

// Student registration, title shortlisting and title confirmation.
class RegistrationService : public Service {
 public:
  static constexpr std::size_t kMaxShortlist = 3;

  using Service::Service;

  Status registerStudent(const std::string& id, const std::string& name);

  // A student may shortlist up to kMaxShortlist distinct, still-available titles.
  Status addToShortlist(const std::string& studentId, const std::string& titleId);
  Status removeFromShortlist(const std::string& studentId, const std::string& titleId);

  // Lecturer action: confirm one of the student's shortlisted titles for them.
  // The title then becomes unavailable to everyone else.
  Status confirm(const std::string& studentId, const std::string& titleId);

  std::vector<const Title*> shortlist(const std::string& studentId) const;
  const Title* confirmedTitle(const std::string& studentId) const { return db_.titleOf(studentId); }
};

}  // namespace fyp
