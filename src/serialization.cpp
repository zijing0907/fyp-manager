#include "fyp/serialization.hpp"

#include <algorithm>
#include <istream>
#include <ostream>
#include <set>
#include <sstream>

#include "fyp/rubric.hpp"
#include "fyp/util.hpp"

namespace fyp {

namespace {

// Calls fn(lineNumber, fields) for each data line.
template <typename Fn>
void forEachRecord(std::istream& in, Fn fn) {
  std::string line;
  int lineNumber = 0;
  while (std::getline(in, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF files
    if (util::trim(line).empty() || line[0] == '#') continue;
    fn(lineNumber, util::splitFields(line));
  }
}

std::string where(const std::string& source, int lineNumber) {
  return source + ":" + std::to_string(lineNumber) + ": ";
}

// "T" followed by 1-6 digits (the limit keeps the numeric part safely inside an int).
bool looksLikeTitleId(const std::string& id) {
  return id.size() >= 2 && id.size() <= 7 && id[0] == 'T' && util::isAllDigits(std::string_view(id).substr(1));
}

// Shortest faithful text for a mark: 85 -> "85", 85.5 -> "85.5".
std::string compact(double value) {
  std::ostringstream os;
  os.precision(10);
  os << value;
  return os.str();
}

}  // namespace

void readTitles(std::istream& in, const std::string& source, Database& db, Warnings& warnings) {
  forEachRecord(in, [&](int line, std::vector<std::string> f) {
    if (f.size() < 2 || f.size() > 3) {
      warnings.push_back(where(source, line) + "expected \"id|title|assigned_student_id\"; line skipped");
      return;
    }
    Title title{util::trim(f[0]), util::trim(f[1]), f.size() == 3 ? util::trim(f[2]) : ""};
    if (!looksLikeTitleId(title.id) || title.text.empty()) {
      warnings.push_back(where(source, line) + "invalid title id or empty title text; line skipped");
    } else if (db.findTitle(title.id)) {
      warnings.push_back(where(source, line) + "duplicate title id " + title.id + "; line skipped");
    } else {
      db.titles.push_back(std::move(title));
    }
  });
}

void readStudents(std::istream& in, const std::string& source, Database& db, Warnings& warnings) {
  forEachRecord(in, [&](int line, std::vector<std::string> f) {
    if (f.size() != 2) {
      warnings.push_back(where(source, line) + "expected \"student_id|name\"; line skipped");
      return;
    }
    Student student{util::trim(f[0]), util::trim(f[1])};
    if (student.id.size() != 7 || !util::isAllDigits(student.id) || student.name.empty()) {
      warnings.push_back(where(source, line) + "student id must be 7 digits and name non-empty; line skipped");
    } else if (db.findStudent(student.id)) {
      warnings.push_back(where(source, line) + "duplicate student id " + student.id + "; line skipped");
    } else {
      db.students.push_back(std::move(student));
    }
  });
}

void readShortlists(std::istream& in, const std::string& source, Database& db, Warnings& warnings) {
  forEachRecord(in, [&](int line, std::vector<std::string> f) {
    if (f.size() != 2) {
      warnings.push_back(where(source, line) + "expected \"student_id|title_id,title_id,...\"; line skipped");
      return;
    }
    std::vector<std::string>& picks = db.shortlists[util::trim(f[0])];
    for (const std::string& raw : util::splitList(f[1])) {
      std::string id = util::trim(raw);
      if (!id.empty() && std::find(picks.begin(), picks.end(), id) == picks.end()) picks.push_back(id);
    }
  });
}

void readMarks(std::istream& in, const std::string& source, Database& db, Warnings& warnings) {
  forEachRecord(in, [&](int line, std::vector<std::string> f) {
    if (f.size() != 5) {
      warnings.push_back(where(source, line) + "expected 5 fields; line skipped");
      return;
    }
    MarkEntry entry;
    entry.studentId = util::trim(f[0]);
    entry.enteredAt = util::trim(f[4]);
    if (!parsePhase(util::trim(f[1]), entry.phase) || !parseRole(util::trim(f[2]), entry.role)) {
      warnings.push_back(where(source, line) + "unknown phase or role; line skipped");
      return;
    }
    for (const std::string& raw : util::splitList(f[3])) {
      double mark = 0;
      if (!util::parseDecimal(util::trim(raw), mark) || mark > 100.0) {
        warnings.push_back(where(source, line) + "marks must be numbers from 0 to 100; line skipped");
        return;
      }
      entry.marks.push_back(mark);
    }
    if (entry.marks.size() != rubric::criteria(entry.phase, entry.role).size()) {
      warnings.push_back(where(source, line) + "wrong number of marks for this phase and role; line skipped");
    } else if (db.findMarks(entry.studentId, entry.phase, entry.role)) {
      warnings.push_back(where(source, line) + "duplicate marks record; line skipped");
    } else {
      db.marks.push_back(std::move(entry));
    }
  });
}

void writeTitles(std::ostream& out, const Database& db) {
  out << "# id|title|assigned_student_id\n";
  for (const Title& t : db.titles) out << util::joinFields({t.id, t.text, t.studentId}) << '\n';
}

void writeStudents(std::ostream& out, const Database& db) {
  out << "# student_id|name\n";
  for (const Student& s : db.students) out << util::joinFields({s.id, s.name}) << '\n';
}

void writeShortlists(std::ostream& out, const Database& db) {
  out << "# student_id|title_id,title_id,...\n";
  for (const auto& [studentId, titleIds] : db.shortlists) {
    if (titleIds.empty()) continue;
    std::string list;
    for (const std::string& id : titleIds) list += (list.empty() ? "" : ",") + id;
    out << util::joinFields({studentId, list}) << '\n';
  }
}

void writeMarks(std::ostream& out, const Database& db) {
  out << "# student_id|phase|role|marks (in rubric order)|entered_at\n";
  for (const MarkEntry& m : db.marks) {
    std::string list;
    for (double mark : m.marks) list += (list.empty() ? "" : ",") + compact(mark);
    out << util::joinFields({m.studentId, toString(m.phase), toString(m.role), list, m.enteredAt}) << '\n';
  }
}

void repairReferences(Database& db, Warnings& warnings) {
  std::set<std::string> studentsWithTitle;
  for (Title& t : db.titles) {
    if (!t.taken()) continue;
    if (!db.findStudent(t.studentId)) {
      warnings.push_back("Title " + t.id + " was assigned to unknown student " + t.studentId + "; released");
      t.studentId.clear();
    } else if (!studentsWithTitle.insert(t.studentId).second) {
      warnings.push_back("Student " + t.studentId + " held more than one title; released " + t.id);
      t.studentId.clear();
    }
  }

  for (auto it = db.shortlists.begin(); it != db.shortlists.end();) {
    if (!db.findStudent(it->first)) {
      warnings.push_back("Dropped shortlist of unknown student " + it->first);
      it = db.shortlists.erase(it);
      continue;
    }
    auto& picks = it->second;
    picks.erase(std::remove_if(picks.begin(), picks.end(),
                               [&](const std::string& id) {
                                 bool missing = db.findTitle(id) == nullptr;
                                 if (missing) warnings.push_back("Dropped unknown title " + id + " from shortlist of " + it->first);
                                 return missing;
                               }),
                picks.end());
    ++it;
  }

  db.marks.erase(std::remove_if(db.marks.begin(), db.marks.end(),
                                [&](const MarkEntry& m) {
                                  bool orphan = db.findStudent(m.studentId) == nullptr;
                                  if (orphan) warnings.push_back("Dropped marks for unknown student " + m.studentId);
                                  return orphan;
                                }),
                 db.marks.end());
}

}  // namespace fyp
