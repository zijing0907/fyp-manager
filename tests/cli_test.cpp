#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>

#include "fyp/cli.hpp"
#include "fyp/rubric.hpp"
#include "fyp/style.hpp"
#include "fyp/util.hpp"
#include "test_support.hpp"

using namespace fyp;

namespace {

// Feeds `script` to the CLI as if typed at the keyboard and returns everything it printed.
std::string runSession(Database& db, Storage& storage, const std::string& script, Cli::Options options = {}) {
  std::istringstream in(script);
  std::ostringstream out;
  Cli cli(db, storage, in, out, [] { return std::string("2026-01-01 00:00:00"); }, options);
  cli.run();
  return out.str();
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

std::size_t countOf(const std::string& haystack, const std::string& needle) {
  std::size_t n = 0;
  for (std::size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1)) ++n;
  return n;
}

}  // namespace

class CliTest : public ::testing::Test {
 protected:
  std::string run(const std::string& script, Cli::Options options = {}) {
    return runSession(db, storage, script, options);
  }

  Database db = support::sampleDb();
  support::MemoryStorage storage;
};

TEST_F(CliTest, ExitsFromTheMainMenu) {
  std::string out = run("e\n");
  EXPECT_TRUE(contains(out, "[1] Lecturer"));
  EXPECT_TRUE(contains(out, "Goodbye"));
}

TEST_F(CliTest, InvalidMenuChoicesAreRejectedAndTheSessionContinues) {
  std::string out = run("9\nx\n\ne\n");
  EXPECT_EQ(countOf(out, "Invalid choice"), 3u);
  EXPECT_TRUE(contains(out, "Goodbye"));
}

TEST_F(CliTest, LecturerCanAddThenSearchATitle) {
  std::string out = run("1\n3\nCompiler Optimisation Toolkit\n2\ncompiler\ne\ne\n");
  EXPECT_TRUE(contains(out, "Added T006."));
  EXPECT_TRUE(contains(out, "RESULTS FOR \"compiler\"  (1 match)"));
  EXPECT_TRUE(contains(out, "T006  Available  Compiler Optimisation Toolkit"));
  EXPECT_EQ(storage.stored.titles.size(), 6u) << "the new title must have been saved";
}

TEST_F(CliTest, LecturerCanModifyATitleByTypingJustItsNumber) {
  std::string out = run("1\n4\n2\nCross-platform Asset Tracker\ne\ne\n");
  EXPECT_TRUE(contains(out, "Current: Mobile Asset Management Application"));
  EXPECT_TRUE(contains(out, "Updated T002."));
  EXPECT_EQ(db.findTitle("T002")->text, "Cross-platform Asset Tracker");
}

TEST_F(CliTest, DeleteAsksForConfirmation) {
  std::string out = run("1\n5\nT003\nn\n5\nT003\ny\ne\ne\n");
  EXPECT_TRUE(contains(out, "Cancelled."));
  EXPECT_TRUE(contains(out, "Deleted T003."));
  EXPECT_EQ(db.findTitle("T003"), nullptr);
  EXPECT_EQ(db.titles.size(), 4u);
}

TEST_F(CliTest, StudentShortlistsByNumberAndSeesItInTheirShortlist) {
  std::string out = run("2\n2400001\n3\n1\n5\ne\ne\n");
  EXPECT_TRUE(contains(out, "Logged in as:  Alice Tan (2400001)"));
  EXPECT_TRUE(contains(out, "Added T001 to your shortlist (1/3)."));
  EXPECT_TRUE(contains(out, "MY SHORTLIST  (1 of 3)"));
  EXPECT_TRUE(contains(out, "  T001  Available  IoT Based Smart Parking System\n"));
  EXPECT_TRUE(contains(out, "Shortlist:     1 / 3")) << "the portal header reflects the new pick";
  EXPECT_EQ(db.shortlists["2400001"], (std::vector<std::string>{"T001"}));
}

TEST_F(CliTest, StudentLoginRejectsMalformedAndUnknownIds) {
  std::string out = run("2\nabc\n240001\n2400009\ne\n");
  EXPECT_EQ(countOf(out, "ID must be exactly 7 digits."), 2u);
  EXPECT_TRUE(contains(out, "Student not found."));
}

TEST_F(CliTest, StudentsSeeWhoHoldsATitleOnlyAsLecturers) {
  db.titles[0].studentId = "2400002";
  std::string student = run("2\n2400001\n1\ne\ne\n");
  EXPECT_TRUE(contains(student, "T001  Taken      IoT Based Smart Parking System"));
  EXPECT_FALSE(contains(student, "held by"));
  std::string lecturer = run("1\n1\ne\ne\n");
  EXPECT_TRUE(contains(lecturer, "T001  Taken      IoT Based Smart Parking System\n"
                                 "                   held by Bob Lim (2400002)"));
}

TEST_F(CliTest, FullRegistrationWorkflowAcrossBothRoles) {
  std::string out = run(
      "1\n6\n2400010\nNur Aisyah\ne\n"  // lecturer registers a student
      "2\n2400010\n3\nT004\ne\n"        // student shortlists T004 and logs out
      "1\n8\n2400010\n4\n7\ne\n"        // lecturer confirms it and lists students
      "e\n");
  EXPECT_TRUE(contains(out, "Registered Nur Aisyah (2400010)."));
  EXPECT_TRUE(contains(out, "Confirmed T004 for Nur Aisyah."));
  ASSERT_NE(db.titleOf("2400010"), nullptr);
  EXPECT_EQ(db.titleOf("2400010")->id, "T004");
  EXPECT_TRUE(contains(out, "2400010  Nur Aisyah  1 / 3      T004"));
  EXPECT_EQ(storage.stored.titleOf("2400010")->id, "T004") << "confirmation must be persisted";
}

TEST_F(CliTest, RegisterRetriesAfterAValidationError) {
  std::string out = run("1\n6\n123\nBad Id\n2400010\nR2D2\n2400010\nGood Name\ne\ne\n");
  EXPECT_TRUE(contains(out, "Error: Student ID must be exactly 7 digits."));
  EXPECT_TRUE(contains(out, "Error: Name may only contain letters"));
  EXPECT_TRUE(contains(out, "Registered Good Name (2400010)."));
}

TEST_F(CliTest, MarkEntryRepromptsOnBadInputThenSaves) {
  db.titles[0].studentId = "2400001";
  std::string out = run("1\n9\n2400001\n1\n1\nabc\n-5\n150\n85\n80\n90\ne\ne\n");
  EXPECT_TRUE(contains(out, "Please enter a number"));
  EXPECT_TRUE(contains(out, "Marks must be between 0 and 100."));
  EXPECT_TRUE(contains(out, "Saved FYP1 supervisor marks for Alice Tan."));
  ASSERT_NE(db.findMarks("2400001", Phase::Fyp1, Role::Supervisor), nullptr);
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Supervisor)->marks, (std::vector<double>{85, 80, 90}));
}

TEST_F(CliTest, ModeratorIsAskedForOneFewerCriterionThanTheSupervisor) {
  db.titles[0].studentId = "2400001";
  std::string out = run("1\n9\n2400001\n2\n1\n80\n70\ne\ne\n");  // moderator, FYP1: two marks only
  EXPECT_TRUE(contains(out, "Report       (0-100")) << "criterion names are padded so the prompts line up";
  EXPECT_TRUE(contains(out, "Presentation (0-100"));
  EXPECT_FALSE(contains(out, "Effort"));
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Moderator)->marks, (std::vector<double>{80, 70}));
}

TEST_F(CliTest, MarkEntryWarnsOnLargeGapAndAsksBeforeReplacing) {
  db.titles[0].studentId = "2400001";
  std::string out = run(
      "1\n9\n2400001\n1\n1\n100\n100\n100\n"   // supervisor: 100
      "9\n2400001\n2\n1\n60\n60\n"              // moderator: 60 -> gap of 40
      "9\n2400001\n2\n1\nn\n"                   // asked to replace, declines
      "e\ne\n");
  EXPECT_TRUE(contains(out, "WARNING: supervisor (100.00) and moderator (60.00) scores differ by 40.00"));
  EXPECT_TRUE(contains(out, "marks were already entered on 2026-01-01 00:00:00."));
  EXPECT_EQ(db.findMarks("2400001", Phase::Fyp1, Role::Moderator)->marks, (std::vector<double>{60, 60}));
}

TEST_F(CliTest, MarkEntryIsRefusedForStudentsWithoutATitle) {
  std::string out = run("1\n9\n2400001\ne\ne\n");
  EXPECT_TRUE(contains(out, "has not been confirmed to a title yet."));
  EXPECT_TRUE(db.marks.empty());
}

TEST_F(CliTest, CancellingHalfwayThroughMarkEntrySavesNothing) {
  db.titles[0].studentId = "2400001";
  std::string out = run("1\n9\n2400001\n1\n1\n90\ne\ne\ne\n");
  EXPECT_TRUE(contains(out, "Nothing was saved."));
  EXPECT_TRUE(db.marks.empty());
  EXPECT_EQ(storage.saveCount, 0);
}

TEST_F(CliTest, StudentResultAndClassSummaryReportOutcomes) {
  db.titles[0].studentId = "2400001";
  db.titles[1].studentId = "2400002";
  GradingService grading(db, storage, [] { return std::string("t"); });
  for (Phase p : {Phase::Fyp1, Phase::Fyp2}) {
    for (Role r : {Role::Supervisor, Role::Moderator}) {
      ASSERT_TRUE(grading.enterMarks("2400001", p, r, std::vector<double>(rubric::criteria(p, r).size(), 90)));
    }
  }
  std::string out = run("1\n10\n2400001\n10\n2400002\n11\ne\ne\n");
  EXPECT_TRUE(contains(out, "Final mark: 90.00  ->  Distinction"));
  EXPECT_TRUE(contains(out, "Final mark: not available yet (waiting for: FYP1 supervisor, FYP1 moderator, FYP2 supervisor, FYP2 moderator)"));
  EXPECT_TRUE(contains(out, "CLASS SUMMARY  (2 students)"));
  // The bar chart scales the most common outcome to 24 columns.
  EXPECT_TRUE(contains(out, "  Distinction  " + std::string(24, '#') + "  1\n"));
  EXPECT_TRUE(contains(out, "  Pass         " + std::string(24, ' ') + "  0\n"));
  EXPECT_TRUE(contains(out, "  Incomplete   " + std::string(24, '#') + "  1\n"));
}

TEST_F(CliTest, InputEndingAtAnyPromptShutsDownCleanly) {
  const std::vector<std::string> truncated = {"", "1\n", "1\n9\n", "1\n9\n2400001\n", "2\n2400001\n3\n", "1\n3\n"};
  for (const std::string& script : truncated) {
    Database fresh = support::sampleDb();
    fresh.titles[0].studentId = "2400001";
    support::MemoryStorage st;
    std::string out = runSession(fresh, st, script);
    EXPECT_TRUE(contains(out, "Goodbye")) << "script: " << script;
  }
}

// --- look and feel ----------------------------------------------------------

namespace {

// Visits every screen: menus, title list and search, student list, mark entry
// with a gap warning, a result with a pending phase, an input error, the class
// summary, and the student portal with and without a confirmed title.
const char* const kTourScript =
    "1\n1\n2\nsystem\n7\n"
    "9\n2400002\n1\n1\n100\n100\n100\n"
    "9\n2400002\n2\n1\n50\n50\n"
    "10\nabc\n2400001\n11\ne\n"
    "2\n2400001\n1\n2\nparking\n3\n5\ne\n"
    "2\n2400003\n3\n4\n5\ne\n"
    "e\n";

Database gradedDb() {
  Database db = support::sampleDb();
  db.titles[0].studentId = "2400001";
  db.titles[1].studentId = "2400002";
  db.marks = {{"2400001", Phase::Fyp1, Role::Supervisor, {90, 80, 70}, "t"},
              {"2400001", Phase::Fyp1, Role::Moderator, {60, 60}, "t"}};
  return db;
}

}  // namespace

TEST_F(CliTest, PlainOutputIsAsciiWithoutEscapeCodes) {
  db = gradedDb();
  std::string out = run(kTourScript);
  EXPECT_FALSE(contains(out, "\x1b")) << "piped output and tests must never see ANSI codes";
  EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; }))
      << "plain output must be pure ASCII";
}

TEST_F(CliTest, ColourChangesOnlyHowTheTextLooks) {
  db = gradedDb();
  const std::string plain = run(kTourScript);

  db = gradedDb();
  storage = support::MemoryStorage{};
  Cli::Options coloured;
  coloured.style = Style(true, false);  // colour, ASCII glyphs
  const std::string out = run(kTourScript, coloured);

  EXPECT_TRUE(contains(out, "\x1b[")) << "colour was requested";
  EXPECT_EQ(stripAnsi(out), plain) << "removing the colour codes must give back the plain session, character for character";
}

TEST_F(CliTest, FancyStyleUsesUnicodeGlyphs) {
  db = gradedDb();
  Cli::Options options;
  options.style = Style::fancy();
  std::string out = run("1\n11\ne\ne\n", options);
  EXPECT_TRUE(contains(out, "\xE2\x96\x88")) << "bar chart uses the full block glyph";
  EXPECT_TRUE(contains(out, "\xC2\xB7")) << "banner subtitle uses a middle dot";
}

TEST_F(CliTest, LongTitlesWrapUnderTheTitleColumn) {
  db.titles.push_back({"T006", "Design and Evaluation of an Extremely Long Final Year Project Title That Keeps Going "
                               "Well Past the Edge of an Ordinary Terminal Window",
                       ""});
  std::string out = run("1\n1\ne\ne\n");
  EXPECT_TRUE(contains(out, "  T006  Available  Design and Evaluation of an Extremely Long Final Year\n"
                            "                   Project Title That Keeps Going Well Past the Edge of\n"
                            "                   an Ordinary Terminal Window\n"));
  std::istringstream lines(out);
  for (std::string line; std::getline(lines, line);) {
    EXPECT_LE(util::displayWidth(line), 72u) << "line too wide: " << line;
  }
}

TEST_F(CliTest, PauseAfterActionsWaitsForEnterOnlyAfterRealActions) {
  Cli::Options options;
  options.pauseAfterActions = true;
  std::string out = run("1\n7\n\n99\ne\ne\n", options);  // list students, Enter, invalid choice, back, exit
  EXPECT_EQ(countOf(out, "Press Enter to return to the menu..."), 1u) << "an invalid choice does not pause";
  EXPECT_TRUE(contains(out, "Invalid choice"));
  EXPECT_TRUE(contains(out, "Goodbye"));

  EXPECT_FALSE(contains(run("1\n7\ne\ne\n"), "Press Enter")) << "scripted sessions do not pause by default";
}
