#include <gtest/gtest.h>

#include "fyp/registration_service.hpp"
#include "test_support.hpp"

using namespace fyp;

class RegistrationTest : public ::testing::Test {
 protected:
  Database db = support::sampleDb();
  support::MemoryStorage storage;
  RegistrationService service{db, storage};
};

// ---- registration -----------------------------------------------------------

TEST_F(RegistrationTest, RegistersAValidStudentAndNormalisesTheName) {
  Status s = service.registerStudent(" 2400010 ", "  Nur   Aisyah  binti Ali ");
  ASSERT_TRUE(s) << s.message;
  ASSERT_NE(db.findStudent("2400010"), nullptr);
  EXPECT_EQ(db.findStudent("2400010")->name, "Nur Aisyah binti Ali");
  EXPECT_EQ(storage.saveCount, 1);
}

TEST_F(RegistrationTest, AcceptsApostrophesHyphensAndNonAsciiNames) {
  EXPECT_TRUE(service.registerStudent("2400011", "Jean-Luc O'Brien Jr."));
  EXPECT_TRUE(service.registerStudent("2400012", "陈子敬"));
}

TEST_F(RegistrationTest, RejectsInvalidIdsAndNames) {
  EXPECT_FALSE(service.registerStudent("240001", "Too Short"));
  EXPECT_FALSE(service.registerStudent("24000123", "Too Long"));
  EXPECT_FALSE(service.registerStudent("24000AB", "Not Digits"));
  EXPECT_FALSE(service.registerStudent("2400020", "   "));
  EXPECT_FALSE(service.registerStudent("2400020", "R2D2"));
  EXPECT_FALSE(service.registerStudent("2400020", std::string(61, 'a')));
  EXPECT_FALSE(service.registerStudent("2400001", "Duplicate Id"));
  EXPECT_EQ(db.students.size(), 3u);
  EXPECT_EQ(storage.saveCount, 0);
}

// ---- shortlist ---------------------------------------------------------------

TEST_F(RegistrationTest, ShortlistKeepsPickOrderAndPersists) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T003"));
  Status s = service.addToShortlist("2400001", "T001");
  ASSERT_TRUE(s);
  EXPECT_NE(s.message.find("2/3"), std::string::npos);
  auto picks = service.shortlist("2400001");
  ASSERT_EQ(picks.size(), 2u);
  EXPECT_EQ(picks[0]->id, "T003");
  EXPECT_EQ(picks[1]->id, "T001");
  EXPECT_EQ(storage.saveCount, 2);
}

TEST_F(RegistrationTest, ShortlistIsCappedAndRejectsDuplicatesAndUnavailableTitles) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T001"));
  EXPECT_FALSE(service.addToShortlist("2400001", "T001")) << "duplicate";
  ASSERT_TRUE(service.addToShortlist("2400001", "T002"));
  ASSERT_TRUE(service.addToShortlist("2400001", "T003"));
  Status full = service.addToShortlist("2400001", "T004");
  EXPECT_FALSE(full);
  EXPECT_NE(full.message.find("full"), std::string::npos);

  db.titles[4].studentId = "2400002";  // T005 taken by someone else
  EXPECT_FALSE(service.addToShortlist("2400003", "T005"));
  EXPECT_FALSE(service.addToShortlist("2400003", "T404"));
  EXPECT_FALSE(service.addToShortlist("9999999", "T001"));
}

TEST_F(RegistrationTest, RemoveFromShortlistOnlyRemovesThatTitle) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T001"));
  ASSERT_TRUE(service.addToShortlist("2400001", "T002"));
  ASSERT_TRUE(service.removeFromShortlist("2400001", "T001"));
  ASSERT_EQ(service.shortlist("2400001").size(), 1u);
  EXPECT_EQ(service.shortlist("2400001")[0]->id, "T002");
  EXPECT_FALSE(service.removeFromShortlist("2400001", "T001")) << "already removed";
  EXPECT_FALSE(service.removeFromShortlist("2400002", "T001")) << "never shortlisted anything";
}

// ---- confirmation --------------------------------------------------------------

TEST_F(RegistrationTest, ConfirmAssignsTheTitleAndKeepsItOffOtherStudentsOptions) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T002"));
  ASSERT_TRUE(service.addToShortlist("2400002", "T002"));

  Status s = service.confirm("2400001", "T002");
  ASSERT_TRUE(s) << s.message;
  ASSERT_NE(service.confirmedTitle("2400001"), nullptr);
  EXPECT_EQ(service.confirmedTitle("2400001")->id, "T002");
  EXPECT_EQ(db.findTitle("T002")->studentId, "2400001");

  EXPECT_FALSE(service.confirm("2400002", "T002")) << "already given to Alice";
  EXPECT_EQ(service.confirmedTitle("2400002"), nullptr) << "Bob must not inherit Alice's title";
  EXPECT_FALSE(service.addToShortlist("2400003", "T002"));
}

TEST_F(RegistrationTest, ConfirmRequiresTheTitleToBeOnTheStudentsShortlist) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T001"));
  EXPECT_FALSE(service.confirm("2400001", "T002"));
  EXPECT_FALSE(service.confirm("2400002", "T001")) << "Bob has an empty shortlist";
  EXPECT_FALSE(service.confirm("2400001", "T404"));
  EXPECT_FALSE(service.confirm("9999999", "T001"));
  EXPECT_EQ(storage.saveCount, 1) << "only the successful shortlist add should have saved";
}

TEST_F(RegistrationTest, StudentWithAConfirmedTitleCannotConfirmAnotherOrReshortlist) {
  ASSERT_TRUE(service.addToShortlist("2400001", "T001"));
  ASSERT_TRUE(service.addToShortlist("2400001", "T002"));
  ASSERT_TRUE(service.confirm("2400001", "T001"));
  EXPECT_FALSE(service.confirm("2400001", "T002"));
  EXPECT_FALSE(service.addToShortlist("2400001", "T003"));
}
