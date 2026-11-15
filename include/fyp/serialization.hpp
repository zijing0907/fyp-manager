#pragma once
// Text-file record formats. One '|'-delimited record per line; blank lines and
// lines starting with '#' are ignored; both LF and CRLF line endings are read.
//
//   titles.txt      id | title | assigned_student_id (empty = available)
//   students.txt    student_id | name
//   shortlists.txt  student_id | title_id,title_id,...
//   grades.txt      student_id | FYP1/FYP2 | supervisor/moderator | mark,mark,... | timestamp
//
// Readers never throw on bad data: a malformed record is skipped and reported
// through `warnings` so one corrupted line cannot make the whole system unusable.

#include <iosfwd>
#include <string>
#include <vector>

#include "fyp/models.hpp"

namespace fyp {

using Warnings = std::vector<std::string>;

// `source` is the file name used in warning messages, e.g. "titles.txt".
void readTitles(std::istream& in, const std::string& source, Database& db, Warnings& warnings);
void readStudents(std::istream& in, const std::string& source, Database& db, Warnings& warnings);
void readShortlists(std::istream& in, const std::string& source, Database& db, Warnings& warnings);
void readMarks(std::istream& in, const std::string& source, Database& db, Warnings& warnings);

void writeTitles(std::ostream& out, const Database& db);
void writeStudents(std::ostream& out, const Database& db);
void writeShortlists(std::ostream& out, const Database& db);
void writeMarks(std::ostream& out, const Database& db);

// Enforces cross-file consistency after loading: drops shortlist/grade records
// for unknown students or titles, and un-assigns titles whose student does not
// exist (or who already holds another title). Each repair adds a warning.
void repairReferences(Database& db, Warnings& warnings);

}  // namespace fyp
