// Each test here pins down a defect that was found (and reproduced) in the
// original coursework program, so it cannot quietly come back.

#include <gtest/gtest.h>

#include <sstream>

#include "fyp/cli.hpp"
#include "fyp/grading_service.hpp"
#include "fyp/registration_service.hpp"
#include "fyp/rubric.hpp"
#include "fyp/title_service.hpp"
#include "test_support.hpp"

using namespace fyp;

// Original bug: at end-of-input `cin >> choice` failed but the loop kept
// running, printing "Invalid choice" forever (about 156 MB in 3 seconds).
TEST(Regression, ProgramTerminatesWhenInputEnds) {
  Database db = support::sampleDb();
  support::MemoryStorage storage;
  std::istringstream in("");
  std::ostringstream out;
  Cli cli(db, storage, in, out, [] { return std::string("t"); }, {});
  cli.run();
  EXPECT_LT(out.str().size(), 1000u) << "output must not keep growing after input closes";
}

// Original bug: only a "Taken" flag was saved. On the next start, any student
// who had shortlisted a taken title was treated as if they owned it.
TEST(Regression, ConfirmedTitleSurvivesARestartAndIsNotInheritedByOtherStudents) {
  support::TempDir dir;
  {
    FileStorage storage(dir.path());
    Database db = support::sampleDb();
    RegistrationService service(db, storage);
    ASSERT_TRUE(service.addToShortlist("2400001", "T001"));
    ASSERT_TRUE(service.addToShortlist("2400002", "T001"));  // Bob shortlists the same title
    ASSERT_TRUE(service.confirm("2400001", "T001"));         // ...but Alice is confirmed to it
  }

  FileStorage storage(dir.path());
  Warnings warnings;
  Database reloaded = storage.load(warnings);
  EXPECT_TRUE(warnings.empty());
  RegistrationService service(reloaded, storage);
  ASSERT_NE(service.confirmedTitle("2400001"), nullptr);
  EXPECT_EQ(service.confirmedTitle("2400001")->id, "T001");
  EXPECT_EQ(service.confirmedTitle("2400002"), nullptr) << "Bob never confirmed anything";
}

// Original bug: shortlists stored array positions, so deleting any title shifted
// every later position and silently rewrote other students' shortlists.
TEST(Regression, DeletingATitleDoesNotSilentlyChangeAnyonesShortlist) {
  Database db = support::sampleDb();
  support::MemoryStorage storage;
  TitleService titles(db, storage);
  RegistrationService registration(db, storage);
  ASSERT_TRUE(registration.addToShortlist("2400001", "T003"));
  ASSERT_TRUE(registration.addToShortlist("2400001", "T004"));
  ASSERT_TRUE(registration.addToShortlist("2400001", "T005"));

  ASSERT_TRUE(titles.remove("T001"));

  auto picks = registration.shortlist("2400001");
  ASSERT_EQ(picks.size(), 3u);
  EXPECT_EQ(picks[0]->text, "Web Application for Person Counting");
  EXPECT_EQ(picks[1]->text, "Stock Screening and Recommendation System");
  EXPECT_EQ(picks[2]->text, "Gamified Mood and Wellness Journal");
}

// Original bug: missing marks were stored as -1 and fed straight into the
// formulas, so a student with no FYP2 marks showed "FYP2 = 1.50" and a "Fail".
TEST(Regression, MissingMarksNeverProduceABogusScoreOrAFail) {
  Database db = support::sampleDb();
  db.titles[0].studentId = "2400001";
  support::MemoryStorage storage;
  GradingService grading(db, storage, [] { return std::string("t"); });
  ASSERT_TRUE(grading.enterMarks("2400001", Phase::Fyp1, Role::Supervisor, {100, 100, 100}));
  ASSERT_TRUE(grading.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {60, 60}));

  auto result = grading.resultFor("2400001");
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->fyp2.average());
  EXPECT_FALSE(result->finalMark);
  EXPECT_EQ(result->outcome, Outcome::Incomplete);
  EXPECT_EQ(grading.summarise().fail, 0);
  EXPECT_EQ(grading.summarise().incomplete, 1);
}

// Original bug: on a >5 gap the program overwrote both raw marks with their
// average ("Adjusted to average: 22.5"), and the report then applied its own
// offset on top, so it printed 25.00 for the same student.
TEST(Regression, TheAverageShownInWarningsMatchesTheAverageInReports) {
  Database db = support::sampleDb();
  db.titles[0].studentId = "2400001";
  support::MemoryStorage storage;
  GradingService grading(db, storage, [] { return std::string("t"); });
  ASSERT_TRUE(grading.enterMarks("2400001", Phase::Fyp1, Role::Supervisor, {100, 100, 100}));
  Status s = grading.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {60, 60});
  ASSERT_TRUE(s);

  EXPECT_DOUBLE_EQ(*grading.phaseScores("2400001", Phase::Fyp1).average(), 80.0);
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Supervisor)->marks, (std::vector<double>{100, 100, 100}))
      << "raw marks must survive the moderation warning";

  std::istringstream in("1\n10\n2400001\ne\ne\n");
  std::ostringstream out;
  Cli(db, storage, in, out, [] { return std::string("t"); }, {}).run();

  // The FYP1 row of the printed report must show supervisor 100, moderator 60 and average 80.
  std::string output = out.str();
  std::size_t rowStart = output.find("FYP1 (30%)");
  ASSERT_NE(rowStart, std::string::npos) << output;
  std::string row = output.substr(rowStart, output.find('\n', rowStart) - rowStart);
  EXPECT_NE(row.find("100.00"), std::string::npos) << row;
  EXPECT_NE(row.find("60.00"), std::string::npos) << row;
  EXPECT_NE(row.find("80.00"), std::string::npos) << row;
}

// Original bug: the data file was shipped as "titles.txt.txt", so the program
// failed on first launch. Also guards the assignment's 30-title requirement.
TEST(Regression, ShippedSampleDataLoadsCleanlyAndHasAtLeastThirtyTitles) {
  FileStorage storage(FYP_SAMPLE_DATA_DIR);
  Warnings warnings;
  Database db = storage.load(warnings);
  EXPECT_TRUE(warnings.empty()) << (warnings.empty() ? "" : warnings.front());
  EXPECT_GE(db.titles.size(), 30u);
  EXPECT_FALSE(db.students.empty());
}
