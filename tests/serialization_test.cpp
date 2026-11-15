#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

#include "fyp/serialization.hpp"
#include "fyp/storage.hpp"
#include "test_support.hpp"

using namespace fyp;

namespace {

Database fullDb() {
  Database db = support::sampleDb();
  db.titles[0].studentId = "2400001";
  db.titles[1].text = "Title with | a pipe and a \\ backslash";
  db.shortlists["2400001"] = {"T001", "T003"};
  db.shortlists["2400002"] = {"T005"};
  db.marks.push_back({"2400001", Phase::Fyp1, Role::Supervisor, {85, 80.5, 90}, "2026-01-05 10:00:00"});
  db.marks.push_back({"2400001", Phase::Fyp1, Role::Moderator, {82, 78}, "2026-01-05 11:00:00"});
  return db;
}

Database roundTrip(const Database& original, Warnings& warnings) {
  std::stringstream titles, students, shortlists, marks;
  writeTitles(titles, original);
  writeStudents(students, original);
  writeShortlists(shortlists, original);
  writeMarks(marks, original);

  Database copy;
  readTitles(titles, "titles.txt", copy, warnings);
  readStudents(students, "students.txt", copy, warnings);
  readShortlists(shortlists, "shortlists.txt", copy, warnings);
  readMarks(marks, "grades.txt", copy, warnings);
  return copy;
}

}  // namespace

TEST(Serialization, RoundTripPreservesEverything) {
  Database original = fullDb();
  Warnings warnings;
  Database copy = roundTrip(original, warnings);

  EXPECT_TRUE(warnings.empty());
  ASSERT_EQ(copy.titles.size(), original.titles.size());
  for (std::size_t i = 0; i < original.titles.size(); ++i) {
    EXPECT_EQ(copy.titles[i].id, original.titles[i].id);
    EXPECT_EQ(copy.titles[i].text, original.titles[i].text);
    EXPECT_EQ(copy.titles[i].studentId, original.titles[i].studentId);
  }
  EXPECT_EQ(copy.students.size(), 3u);
  EXPECT_EQ(copy.shortlists, original.shortlists);
  ASSERT_EQ(copy.marks.size(), 2u);
  EXPECT_EQ(copy.marks[0].marks, (std::vector<double>{85, 80.5, 90}));
  EXPECT_EQ(copy.marks[1].role, Role::Moderator);
  EXPECT_EQ(copy.marks[1].enteredAt, "2026-01-05 11:00:00");
}

TEST(Serialization, ReadsWindowsLineEndings) {
  // The original coursework data file had CRLF endings, which broke its status check.
  std::istringstream in("T001|First title here|2400001\r\nT002|Second title here|\r\n");
  Database db;
  Warnings warnings;
  readTitles(in, "titles.txt", db, warnings);
  ASSERT_EQ(db.titles.size(), 2u);
  EXPECT_EQ(db.titles[0].studentId, "2400001") << "no stray \\r may end up in the last field";
  EXPECT_FALSE(db.titles[1].taken());
  EXPECT_TRUE(warnings.empty());
}

TEST(Serialization, IgnoresCommentsBlankLinesAndAcceptsTitleWithoutStudentField) {
  std::istringstream in("# a comment\n\n   \nT001|Only two fields here\n");
  Database db;
  Warnings warnings;
  readTitles(in, "titles.txt", db, warnings);
  ASSERT_EQ(db.titles.size(), 1u);
  EXPECT_FALSE(db.titles[0].taken());
  EXPECT_TRUE(warnings.empty());
}

TEST(Serialization, MalformedTitleLinesAreSkippedWithLineNumberedWarnings) {
  std::istringstream in(
      "T001|Good title here|\n"
      "no delimiter at all\n"
      "X002|Bad id prefix|\n"
      "T003|   |\n"
      "T001|Duplicate id|\n"
      "T004|One|two|three|four\n"
      "T12345678|Id with too many digits|\n");
  Database db;
  Warnings warnings;
  readTitles(in, "titles.txt", db, warnings);

  EXPECT_EQ(db.titles.size(), 1u);
  ASSERT_EQ(warnings.size(), 6u);
  EXPECT_NE(warnings[0].find("titles.txt:2:"), std::string::npos);
  EXPECT_NE(warnings[3].find("duplicate"), std::string::npos);
}

TEST(Serialization, StudentReaderValidatesIds) {
  std::istringstream in("2400001|Alice Tan\n123|Short Id\n24000AB|Letters\n2400001|Duplicate\n2400002|\n");
  Database db;
  Warnings warnings;
  readStudents(in, "students.txt", db, warnings);
  EXPECT_EQ(db.students.size(), 1u);
  EXPECT_EQ(warnings.size(), 4u);
}

TEST(Serialization, MarkReaderRejectsBadRecords) {
  std::istringstream in(
      "2400001|FYP1|supervisor|85,80,90|t\n"       // ok
      "2400001|FYP1|moderator|82,78,99|t\n"        // moderators have only two criteria
      "2400001|FYP2|moderator|82,78,101|t\n"       // out of range
      "2400001|FYP3|moderator|82,78,70|t\n"        // unknown phase
      "2400001|FYP1|supervisor|1,2,3|t\n"          // duplicate
      "2400001|FYP2|supervisor|a,b,c,d|t\n"        // not numbers
      "2400001|FYP1\n");                           // too few fields
  Database db;
  Warnings warnings;
  readMarks(in, "grades.txt", db, warnings);
  EXPECT_EQ(db.marks.size(), 1u);
  EXPECT_EQ(warnings.size(), 6u);
}

TEST(Serialization, RepairDropsDanglingReferences) {
  Database db = support::sampleDb();
  db.titles[0].studentId = "9999999";                         // unknown student
  db.titles[1].studentId = "2400001";
  db.titles[2].studentId = "2400001";                         // same student holds a second title
  db.shortlists["8888888"] = {"T001"};                        // unknown student
  db.shortlists["2400002"] = {"T004", "T404", "T005"};        // T404 does not exist
  db.marks.push_back({"7777777", Phase::Fyp1, Role::Supervisor, {1, 2, 3}, "t"});

  Warnings warnings;
  repairReferences(db, warnings);

  EXPECT_FALSE(db.titles[0].taken());
  EXPECT_EQ(db.titles[1].studentId, "2400001");
  EXPECT_FALSE(db.titles[2].taken());
  EXPECT_EQ(db.shortlists.count("8888888"), 0u);
  EXPECT_EQ(db.shortlists["2400002"], (std::vector<std::string>{"T004", "T005"}));
  EXPECT_TRUE(db.marks.empty());
  EXPECT_EQ(warnings.size(), 5u);
}

TEST(FileStorage, SaveThenLoadRoundTripsAndLeavesNoTempFiles) {
  support::TempDir dir;
  FileStorage storage(dir.path() / "nested" / "data");  // directory does not exist yet
  Database original = fullDb();
  storage.save(original);

  for (const auto& entry : std::filesystem::directory_iterator(dir.path() / "nested" / "data")) {
    EXPECT_NE(entry.path().extension(), ".tmp") << entry.path();
  }

  Warnings warnings;
  Database loaded = FileStorage(dir.path() / "nested" / "data").load(warnings);
  EXPECT_TRUE(warnings.empty());
  EXPECT_EQ(loaded.titles.size(), original.titles.size());
  EXPECT_EQ(loaded.titles[1].text, original.titles[1].text);
  EXPECT_EQ(loaded.titles[0].studentId, "2400001");
  EXPECT_EQ(loaded.marks.size(), 2u);
}

TEST(FileStorage, MissingDirectoryLoadsEmptyWithAWarning) {
  support::TempDir dir;
  Warnings warnings;
  Database db = FileStorage(dir.path() / "does_not_exist").load(warnings);
  EXPECT_TRUE(db.titles.empty());
  ASSERT_EQ(warnings.size(), 1u);
  EXPECT_NE(warnings[0].find("titles.txt not found"), std::string::npos);
}

TEST(FileStorage, SaveFailureThrowsAndKeepsPreviousFile) {
  support::TempDir dir;
  FileStorage storage(dir.path());
  Database db = fullDb();
  storage.save(db);

  // Block the temp-file path with a directory so the next save cannot write it.
  std::filesystem::create_directory(dir.path() / "grades.txt.tmp");
  db.titles.push_back({"T006", "Extra title added later", ""});
  EXPECT_THROW(storage.save(db), std::runtime_error);

  Warnings warnings;
  Database loaded = FileStorage(dir.path()).load(warnings);
  EXPECT_EQ(loaded.marks.size(), 2u) << "the previous grades.txt must survive a failed save";
}
