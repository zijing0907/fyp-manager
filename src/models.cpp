#include "fyp/models.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "fyp/util.hpp"

namespace fyp {

const char* toString(Phase phase) { return phase == Phase::Fyp1 ? "FYP1" : "FYP2"; }
const char* toString(Role role) { return role == Role::Supervisor ? "supervisor" : "moderator"; }

bool parsePhase(const std::string& text, Phase& out) {
  if (text == "FYP1") { out = Phase::Fyp1; return true; }
  if (text == "FYP2") { out = Phase::Fyp2; return true; }
  return false;
}

bool parseRole(const std::string& text, Role& out) {
  if (text == "supervisor") { out = Role::Supervisor; return true; }
  if (text == "moderator") { out = Role::Moderator; return true; }
  return false;
}

std::string makeTitleId(int number) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "T%03d", number);
  return buf;
}

std::string normaliseTitleId(const std::string& input) {
  std::string s = util::trim(input);
  std::string digits = (!s.empty() && (s[0] == 'T' || s[0] == 't')) ? s.substr(1) : s;
  if (util::isAllDigits(digits) && digits.size() <= 6) return makeTitleId(std::stoi(digits));
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

const Title* Database::findTitle(const std::string& id) const {
  auto it = std::find_if(titles.begin(), titles.end(), [&](const Title& t) { return t.id == id; });
  return it == titles.end() ? nullptr : &*it;
}

Title* Database::findTitle(const std::string& id) {
  return const_cast<Title*>(static_cast<const Database*>(this)->findTitle(id));
}

const Student* Database::findStudent(const std::string& id) const {
  auto it = std::find_if(students.begin(), students.end(), [&](const Student& s) { return s.id == id; });
  return it == students.end() ? nullptr : &*it;
}

const Title* Database::titleOf(const std::string& studentId) const {
  auto it = std::find_if(titles.begin(), titles.end(), [&](const Title& t) { return t.studentId == studentId; });
  return it == titles.end() ? nullptr : &*it;
}

const MarkEntry* Database::findMarks(const std::string& studentId, Phase phase, Role role) const {
  auto it = std::find_if(marks.begin(), marks.end(), [&](const MarkEntry& m) {
    return m.studentId == studentId && m.phase == phase && m.role == role;
  });
  return it == marks.end() ? nullptr : &*it;
}

MarkEntry* Database::findMarks(const std::string& studentId, Phase phase, Role role) {
  return const_cast<MarkEntry*>(static_cast<const Database*>(this)->findMarks(studentId, phase, role));
}

}  // namespace fyp
