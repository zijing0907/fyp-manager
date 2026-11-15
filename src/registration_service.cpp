#include "fyp/registration_service.hpp"

#include <algorithm>
#include <cctype>

#include "fyp/util.hpp"

namespace fyp {

namespace {

// Letters (including UTF-8 multi-byte characters), spaces, apostrophes, hyphens and dots.
bool isValidName(const std::string& name) {
  return std::all_of(name.begin(), name.end(), [](char ch) {
    auto c = static_cast<unsigned char>(ch);
    return std::isalpha(c) || c >= 0x80 || c == ' ' || c == '\'' || c == '-' || c == '.';
  });
}

std::string collapseSpaces(const std::string& s) {
  std::string out;
  for (const std::string& word : util::splitWhitespace(s)) out += (out.empty() ? "" : " ") + word;
  return out;
}

}  // namespace

Status RegistrationService::registerStudent(const std::string& rawId, const std::string& rawName) {
  std::string id = util::trim(rawId);
  std::string name = collapseSpaces(rawName);
  if (id.size() != 7 || !util::isAllDigits(id)) return Status::failure("Student ID must be exactly 7 digits.");
  if (name.empty()) return Status::failure("Name cannot be blank.");
  if (name.size() > 60) return Status::failure("Name must be at most 60 characters.");
  if (!isValidName(name)) return Status::failure("Name may only contain letters, spaces, apostrophes, hyphens and dots.");
  if (db_.findStudent(id)) return Status::failure("Student ID " + id + " is already registered.");

  db_.students.push_back({id, name});
  commit();
  return Status::success("Registered " + name + " (" + id + ").");
}

Status RegistrationService::addToShortlist(const std::string& studentId, const std::string& titleId) {
  if (!db_.findStudent(studentId)) return Status::failure("Student " + studentId + " not found.");
  if (db_.titleOf(studentId)) return Status::failure("You already have a confirmed title; the shortlist is closed.");
  const Title* title = db_.findTitle(titleId);
  if (!title) return Status::failure("Title " + titleId + " not found.");
  if (title->taken()) return Status::failure(titleId + " is already taken by another student.");

  std::vector<std::string>& picks = db_.shortlists[studentId];
  if (std::find(picks.begin(), picks.end(), titleId) != picks.end()) return Status::failure(titleId + " is already on your shortlist.");
  if (picks.size() >= kMaxShortlist) {
    return Status::failure("Your shortlist is full (" + std::to_string(kMaxShortlist) + " titles). Remove one first.");
  }
  picks.push_back(titleId);
  commit();
  return Status::success("Added " + titleId + " to your shortlist (" + std::to_string(picks.size()) + "/" +
                         std::to_string(kMaxShortlist) + ").");
}

Status RegistrationService::removeFromShortlist(const std::string& studentId, const std::string& titleId) {
  if (!db_.findStudent(studentId)) return Status::failure("Student " + studentId + " not found.");
  auto found = db_.shortlists.find(studentId);
  if (found == db_.shortlists.end()) return Status::failure(titleId + " is not on your shortlist.");
  std::vector<std::string>& picks = found->second;
  auto it = std::find(picks.begin(), picks.end(), titleId);
  if (it == picks.end()) return Status::failure(titleId + " is not on your shortlist.");

  picks.erase(it);
  commit();
  return Status::success("Removed " + titleId + " from your shortlist.");
}

Status RegistrationService::confirm(const std::string& studentId, const std::string& titleId) {
  const Student* student = db_.findStudent(studentId);
  if (!student) return Status::failure("Student " + studentId + " not found.");
  if (const Title* existing = db_.titleOf(studentId)) {
    return Status::failure(student->name + " is already confirmed to " + existing->id + ".");
  }
  Title* title = db_.findTitle(titleId);
  if (!title) return Status::failure("Title " + titleId + " not found.");

  auto listed = db_.shortlists.find(studentId);
  if (listed == db_.shortlists.end() ||
      std::find(listed->second.begin(), listed->second.end(), titleId) == listed->second.end()) {
    return Status::failure(titleId + " is not on " + student->name + "'s shortlist.");
  }
  if (title->taken()) return Status::failure(titleId + " has already been confirmed for another student.");

  title->studentId = studentId;
  commit();
  return Status::success("Confirmed " + titleId + " for " + student->name + ".");
}

std::vector<const Title*> RegistrationService::shortlist(const std::string& studentId) const {
  std::vector<const Title*> result;
  auto found = db_.shortlists.find(studentId);
  if (found == db_.shortlists.end()) return result;
  for (const std::string& id : found->second) {
    if (const Title* t = db_.findTitle(id)) result.push_back(t);
  }
  return result;
}

}  // namespace fyp
