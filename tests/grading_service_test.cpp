#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "fyp/grading_service.hpp"
#include "fyp/rubric.hpp"
#include "test_support.hpp"

using namespace fyp;

// ---- rubric -------------------------------------------------------------------

TEST(Rubric, PerfectMarksScoreOneHundredForEveryRoleAndPhase) {
  for (Phase phase : {Phase::Fyp1, Phase::Fyp2}) {
    for (Role role : {Role::Supervisor, Role::Moderator}) {
      std::vector<double> marks(rubric::criteria(phase, role).size(), 100.0);
      EXPECT_DOUBLE_EQ(rubric::score(phase, role, marks), 100.0);
    }
  }
}

TEST(Rubric, ModeratorsDoNotMarkEffortAndScoresStayComparable) {
  EXPECT_EQ(rubric::criteria(Phase::Fyp1, Role::Supervisor).size(), 3u);
  EXPECT_EQ(rubric::criteria(Phase::Fyp1, Role::Moderator).size(), 2u);
  EXPECT_EQ(rubric::criteria(Phase::Fyp2, Role::Supervisor).size(), 4u);
  EXPECT_EQ(rubric::criteria(Phase::Fyp2, Role::Moderator).size(), 3u);
  // Identical performance on the shared criteria gives an identical score for both roles
  // (the original version compared a 30-point supervisor total with a 25-point moderator total).
  EXPECT_DOUBLE_EQ(rubric::score(Phase::Fyp1, Role::Supervisor, {80, 80, 80}), rubric::score(Phase::Fyp1, Role::Moderator, {80, 80}));
}

TEST(Rubric, WeightedScoresMatchHandCalculation) {
  EXPECT_NEAR(rubric::score(Phase::Fyp1, Role::Supervisor, {85, 80, 90}), (85 * 15 + 80 * 10 + 90 * 5) / 30.0, 1e-9);
  EXPECT_NEAR(rubric::score(Phase::Fyp1, Role::Moderator, {82, 78}), 80.4, 1e-9);
  EXPECT_NEAR(rubric::score(Phase::Fyp2, Role::Supervisor, {88, 90, 85, 95}), 6235.0 / 70.0, 1e-9);
  EXPECT_NEAR(rubric::score(Phase::Fyp2, Role::Moderator, {85, 88, 80}), 5580.0 / 65.0, 1e-9);
}

// ---- classification ----------------------------------------------------------------

TEST(Classification, BoundariesAreInclusiveAndUseTheDisplayedTwoDecimals) {
  EXPECT_EQ(classify(75.0), Outcome::Distinction);
  EXPECT_EQ(classify(74.999), Outcome::Distinction) << "shows as 75.00, so it must classify as 75.00";
  EXPECT_EQ(classify(74.994), Outcome::Pass);
  EXPECT_EQ(classify(50.0), Outcome::Pass);
  EXPECT_EQ(classify(49.99), Outcome::Fail);
  EXPECT_EQ(classify(0.0), Outcome::Fail);
  EXPECT_EQ(classify(std::nullopt), Outcome::Incomplete);
}

TEST(PhaseScoresTest, DifferenceIsRoundedSoFloatingPointNoiseCannotTriggerAWarning) {
  PhaseScores p;
  p.supervisor = 64.01;
  p.moderator = 59.01;
  ASSERT_GT(*p.supervisor - *p.moderator, 5.0) << "premise: the raw double subtraction lands just above 5";
  EXPECT_DOUBLE_EQ(*p.difference(), 5.0);
  EXPECT_FALSE(p.discrepancy());
  p.moderator = 58.99;
  EXPECT_TRUE(p.discrepancy());
}

TEST(PhaseScoresTest, IncompleteScoresHaveNoAverageOrDifference) {
  PhaseScores p;
  p.supervisor = 70.0;
  EXPECT_FALSE(p.complete());
  EXPECT_FALSE(p.average());
  EXPECT_FALSE(p.difference());
  EXPECT_FALSE(p.discrepancy());
}

// ---- service ------------------------------------------------------------------------

class GradingTest : public ::testing::Test {
 protected:
  GradingTest() {
    db.titles[0].studentId = "2400001";  // Alice
    db.titles[1].studentId = "2400002";  // Bob
    db.titles[2].studentId = "2400003";  // Chloe
  }

  // Marks every criterion with the same value, so the resulting score is exactly `value`.
  Status enterUniform(const std::string& id, Phase phase, Role role, double value) {
    return service.enterMarks(id, phase, role, std::vector<double>(rubric::criteria(phase, role).size(), value));
  }
  void enterAll(const std::string& id, double fyp1Sup, double fyp1Mod, double fyp2Sup, double fyp2Mod) {
    ASSERT_TRUE(enterUniform(id, Phase::Fyp1, Role::Supervisor, fyp1Sup));
    ASSERT_TRUE(enterUniform(id, Phase::Fyp1, Role::Moderator, fyp1Mod));
    ASSERT_TRUE(enterUniform(id, Phase::Fyp2, Role::Supervisor, fyp2Sup));
    ASSERT_TRUE(enterUniform(id, Phase::Fyp2, Role::Moderator, fyp2Mod));
  }

  Database db = support::sampleDb();
  support::MemoryStorage storage;
  int tick = 0;
  GradingService service{db, storage, [this] { return "2026-01-01 00:00:0" + std::to_string(++tick); }};
};

TEST_F(GradingTest, EnterMarksStoresRawMarksTimestampsAndPersists) {
  Status s = service.enterMarks("2400001", Phase::Fyp1, Role::Supervisor, {85, 80, 90});
  ASSERT_TRUE(s) << s.message;
  const MarkEntry* e = db.findMarks("2400001", Phase::Fyp1, Role::Supervisor);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->marks, (std::vector<double>{85, 80, 90}));
  EXPECT_EQ(e->enteredAt, "2026-01-01 00:00:01");
  EXPECT_EQ(storage.saveCount, 1);
  EXPECT_EQ(s.message.find("WARNING"), std::string::npos);
}

TEST_F(GradingTest, EnterMarksRejectsInvalidRequestsWithoutSaving) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(service.enterMarks("9999999", Phase::Fyp1, Role::Supervisor, {1, 2, 3})) << "unknown student";
  EXPECT_FALSE(service.enterMarks("2400002", Phase::Fyp1, Role::Supervisor, {1, 2})) << "too few marks";
  EXPECT_FALSE(service.enterMarks("2400002", Phase::Fyp1, Role::Moderator, {1, 2, 3})) << "moderators mark 2 criteria";
  EXPECT_FALSE(service.enterMarks("2400002", Phase::Fyp1, Role::Supervisor, {50, 101, 50}));
  EXPECT_FALSE(service.enterMarks("2400002", Phase::Fyp1, Role::Supervisor, {50, -0.5, 50}));
  EXPECT_FALSE(service.enterMarks("2400002", Phase::Fyp1, Role::Supervisor, {50, nan, 50}));
  EXPECT_EQ(storage.saveCount, 0);
  EXPECT_TRUE(db.marks.empty());
}

TEST_F(GradingTest, StudentWithoutAConfirmedTitleCannotBeGraded) {
  Status refused = service.enterMarks("2400001", Phase::Fyp1, Role::Supervisor, {1, 2, 3});
  ASSERT_TRUE(refused) << "sanity check: Alice is confirmed in this fixture";

  db.titles[0].studentId.clear();  // un-assign her title
  refused = service.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {1, 2});
  EXPECT_FALSE(refused);
  EXPECT_NE(refused.message.find("not been confirmed"), std::string::npos);
  EXPECT_EQ(db.marks.size(), 1u) << "only the earlier successful entry exists";
}

TEST_F(GradingTest, EnteringMarksAgainReplacesTheEarlierEntry) {
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {50, 50}));
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {70, 60}));
  EXPECT_EQ(db.marks.size(), 1u);
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Moderator)->marks, (std::vector<double>{70, 60}));
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Moderator)->enteredAt, "2026-01-01 00:00:02");
}

TEST_F(GradingTest, WarnsWhenSupervisorAndModeratorDifferByMoreThanFive) {
  ASSERT_TRUE(enterUniform("2400001", Phase::Fyp1, Role::Supervisor, 80));

  Status exactlyFive = enterUniform("2400001", Phase::Fyp1, Role::Moderator, 75);
  ASSERT_TRUE(exactlyFive);
  EXPECT_EQ(exactlyFive.message.find("WARNING"), std::string::npos) << "a gap of exactly 5 is allowed";

  Status justOver = enterUniform("2400001", Phase::Fyp1, Role::Moderator, 74.9);
  ASSERT_TRUE(justOver) << "a large gap is a warning, not an error: the marks are still saved";
  EXPECT_NE(justOver.message.find("WARNING"), std::string::npos);
  EXPECT_NE(justOver.message.find("80.00"), std::string::npos);
  EXPECT_NE(justOver.message.find("74.90"), std::string::npos);
  EXPECT_NE(justOver.message.find("5.10"), std::string::npos);
}

TEST_F(GradingTest, RawMarksAreNeverOverwrittenByTheAveraging) {
  ASSERT_TRUE(enterUniform("2400001", Phase::Fyp1, Role::Supervisor, 100));
  ASSERT_TRUE(enterUniform("2400001", Phase::Fyp1, Role::Moderator, 60));  // 40 apart
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Supervisor)->marks, (std::vector<double>{100, 100, 100}));
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Moderator)->marks, (std::vector<double>{60, 60}));
  EXPECT_DOUBLE_EQ(*service.phaseScores("2400001", Phase::Fyp1).average(), 80.0);
}

TEST_F(GradingTest, FinalMarkWeightsFyp1At30AndFyp2At70) {
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp1, Role::Supervisor, {85, 80, 90}));
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp1, Role::Moderator, {82, 78}));
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp2, Role::Supervisor, {88, 90, 85, 95}));
  ASSERT_TRUE(service.enterMarks("2400001", Phase::Fyp2, Role::Moderator, {85, 88, 80}));

  auto r = service.resultFor("2400001");
  ASSERT_TRUE(r && r->finalMark);
  double avg1 = (2525.0 / 30.0 + 80.4) / 2.0;
  double avg2 = (6235.0 / 70.0 + 5580.0 / 65.0) / 2.0;
  EXPECT_NEAR(*r->fyp1.average(), avg1, 1e-9);
  EXPECT_NEAR(*r->fyp2.average(), avg2, 1e-9);
  EXPECT_NEAR(*r->finalMark, 0.3 * avg1 + 0.7 * avg2, 1e-9);
  EXPECT_NEAR(*r->finalMark, 85.906, 0.001);
  EXPECT_EQ(r->outcome, Outcome::Distinction);
  EXPECT_FALSE(r->hasDiscrepancy());
}

TEST_F(GradingTest, ResultIsIncompleteUntilAllFourAssessmentsAreIn) {
  ASSERT_TRUE(enterUniform("2400002", Phase::Fyp1, Role::Supervisor, 70));
  ASSERT_TRUE(enterUniform("2400002", Phase::Fyp1, Role::Moderator, 70));
  auto r = service.resultFor("2400002");
  ASSERT_TRUE(r);
  EXPECT_TRUE(r->fyp1.complete());
  EXPECT_FALSE(r->fyp2.supervisor);
  EXPECT_FALSE(r->finalMark);
  EXPECT_EQ(r->outcome, Outcome::Incomplete);
}

TEST_F(GradingTest, ResultForUnknownStudentIsEmpty) { EXPECT_FALSE(service.resultFor("9999999")); }

TEST_F(GradingTest, SummaryCountsOutcomesAndIgnoresStudentsWithoutATitle) {
  db.students.push_back({"2400004", "Dave Ong"});
  db.students.push_back({"2400005", "Erin Goh"});  // never confirmed: not part of the cohort
  db.titles[3].studentId = "2400004";

  enterAll("2400001", 90, 90, 90, 90);  // Distinction, 90
  enterAll("2400002", 60, 50, 60, 60);  // Pass, FYP1 gap of 10 flagged: 0.3*55 + 0.7*60 = 58.5
  enterAll("2400003", 30, 30, 30, 30);  // Fail
  ASSERT_TRUE(enterUniform("2400004", Phase::Fyp1, Role::Supervisor, 80));  // Incomplete

  Summary s = service.summarise();
  EXPECT_EQ(s.students, 4);
  EXPECT_EQ(s.distinction, 1);
  EXPECT_EQ(s.pass, 1);
  EXPECT_EQ(s.fail, 1);
  EXPECT_EQ(s.incomplete, 1);
  EXPECT_EQ(s.discrepancies, 1);
  ASSERT_TRUE(s.mean && s.highest && s.lowest);
  EXPECT_DOUBLE_EQ(*s.mean, 59.5) << "(90 + 58.5 + 30) / 3; the incomplete student is not averaged in";
  EXPECT_DOUBLE_EQ(*s.highest, 90.0);
  EXPECT_DOUBLE_EQ(*s.lowest, 30.0);
}

TEST_F(GradingTest, SummaryOfAnEmptyCohortHasNoStatistics) {
  Database empty = support::sampleDb();
  support::MemoryStorage st;
  GradingService svc(empty, st, [] { return std::string("t"); });
  Summary s = svc.summarise();
  EXPECT_EQ(s.students, 0);
  EXPECT_FALSE(s.mean);
  EXPECT_FALSE(s.highest);
}
