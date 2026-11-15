#pragma once
// Plain data types for the whole system, plus the in-memory Database that the
// services operate on. Everything here is I/O-free so it is trivial to test.

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace fyp {

// Outcome of a service call. `message` is user-facing: the reason on failure,
// or a confirmation (possibly with a warning) on success.
struct Status {
  bool ok = true;
  std::string message;

  explicit operator bool() const { return ok; }
  static Status success(std::string message = {}) { return {true, std::move(message)}; }
  static Status failure(std::string message) { return {false, std::move(message)}; }
};

struct Title {
  std::string id;         // stable identifier such as "T007"; never shifts when other titles are deleted
  std::string text;
  std::string studentId;  // empty while the title is still available

  bool taken() const { return !studentId.empty(); }
};

struct Student {
  std::string id;  // 7 digits, e.g. "2400001"
  std::string name;
};

enum class Phase { Fyp1, Fyp2 };
enum class Role { Supervisor, Moderator };

const char* toString(Phase phase);  // "FYP1" / "FYP2"
const char* toString(Role role);    // "supervisor" / "moderator"
bool parsePhase(const std::string& text, Phase& out);
bool parseRole(const std::string& text, Role& out);

// One assessor's raw criterion marks (0-100 each) for one phase of one student.
// Raw marks are stored, not weighted totals, so the rubric can change without
// invalidating old data.
struct MarkEntry {
  std::string studentId;
  Phase phase = Phase::Fyp1;
  Role role = Role::Supervisor;
  std::vector<double> marks;  // one per applicable criterion, in rubric order
  std::string enteredAt;      // timestamp
};

// "T" + zero-padded number, e.g. 7 -> "T007".
std::string makeTitleId(int number);
// Lets users type "7", "t7" or "T007". Returns the canonical id, or the
// upper-cased input if it does not look like a title id.
std::string normaliseTitleId(const std::string& input);

struct Database {
  std::vector<Title> titles;
  std::vector<Student> students;
  std::map<std::string, std::vector<std::string>> shortlists;  // student id -> title ids in pick order
  std::vector<MarkEntry> marks;

  const Title* findTitle(const std::string& id) const;
  Title* findTitle(const std::string& id);
  const Student* findStudent(const std::string& id) const;
  // The title confirmed for this student, or nullptr.
  const Title* titleOf(const std::string& studentId) const;
  const MarkEntry* findMarks(const std::string& studentId, Phase phase, Role role) const;
  MarkEntry* findMarks(const std::string& studentId, Phase phase, Role role);
};

}  // namespace fyp
