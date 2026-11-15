#include <gtest/gtest.h>

#include "fyp/title_service.hpp"
#include "test_support.hpp"

using namespace fyp;

class TitleServiceTest : public ::testing::Test {
 protected:
  Database db = support::sampleDb();
  support::MemoryStorage storage;
  TitleService service{db, storage};
};

TEST_F(TitleServiceTest, AddAssignsNextIdTrimsTextAndPersists) {
  Status s = service.add("  Blockchain Voting System  ");
  ASSERT_TRUE(s) << s.message;
  EXPECT_EQ(s.message, "Added T006.");
  EXPECT_EQ(db.titles.back().id, "T006");
  EXPECT_EQ(db.titles.back().text, "Blockchain Voting System");
  EXPECT_FALSE(db.titles.back().taken());
  EXPECT_EQ(storage.saveCount, 1);
  EXPECT_EQ(storage.stored.titles.size(), 6u);
}

TEST_F(TitleServiceTest, AddRejectsBadTextWithoutSaving) {
  EXPECT_FALSE(service.add("abc"));
  EXPECT_FALSE(service.add("     "));
  EXPECT_FALSE(service.add(std::string(TitleService::kMaxLength + 1, 'x')));
  Status dup = service.add("gamified MOOD and wellness JOURNAL");
  EXPECT_FALSE(dup);
  EXPECT_NE(dup.message.find("T005"), std::string::npos) << "the message should name the clashing title";
  EXPECT_EQ(db.titles.size(), 5u);
  EXPECT_EQ(storage.saveCount, 0);
}

TEST_F(TitleServiceTest, ModifyChangesTextAndKeepsIdAndAssignment) {
  db.titles[1].studentId = "2400001";
  Status s = service.modify("T002", "Cross-platform Asset Tracking Application");
  ASSERT_TRUE(s) << s.message;
  EXPECT_EQ(db.titles[1].id, "T002");
  EXPECT_EQ(db.titles[1].text, "Cross-platform Asset Tracking Application");
  EXPECT_EQ(db.titles[1].studentId, "2400001") << "editing a title must not un-assign the student";
  EXPECT_EQ(storage.saveCount, 1);
}

TEST_F(TitleServiceTest, ModifyValidatesInput) {
  EXPECT_FALSE(service.modify("T404", "Some Valid New Text"));
  EXPECT_FALSE(service.modify("T001", "hi"));
  EXPECT_FALSE(service.modify("T001", "Mobile Asset Management Application")) << "duplicates another title";
  EXPECT_TRUE(service.modify("T001", "iot based smart parking system")) << "re-casing its own text is fine";
}

TEST_F(TitleServiceTest, RemoveDeletesTitleAndCleansShortlists) {
  db.shortlists["2400001"] = {"T001", "T003"};
  db.shortlists["2400002"] = {"T003"};
  ASSERT_TRUE(service.remove("T003"));
  EXPECT_EQ(db.findTitle("T003"), nullptr);
  EXPECT_EQ(db.titles.size(), 4u);
  EXPECT_EQ(db.shortlists["2400001"], (std::vector<std::string>{"T001"}));
  EXPECT_TRUE(db.shortlists["2400002"].empty());
}

TEST_F(TitleServiceTest, RemoveRefusesTakenOrUnknownTitles) {
  db.titles[0].studentId = "2400001";
  Status taken = service.remove("T001");
  EXPECT_FALSE(taken);
  EXPECT_NE(taken.message.find("2400001"), std::string::npos);
  EXPECT_FALSE(service.remove("T404"));
  EXPECT_EQ(db.titles.size(), 5u);
  EXPECT_EQ(storage.saveCount, 0);
}

TEST_F(TitleServiceTest, NewIdsNeverCollideWithExistingOnesAfterDeletion) {
  ASSERT_TRUE(service.remove("T002"));
  ASSERT_TRUE(service.add("A Brand New Title"));
  EXPECT_EQ(db.titles.back().id, "T006");
  ASSERT_TRUE(service.remove("T006"));
  ASSERT_TRUE(service.add("Another Brand New Title"));
  EXPECT_EQ(db.titles.back().id, "T006") << "the highest id may be reused; no live reference can point at it";
}

TEST_F(TitleServiceTest, SearchIsCaseInsensitiveAndRequiresEveryKeyword) {
  EXPECT_EQ(service.search("APPLICATION").size(), 2u);
  auto both = service.search("web  application");
  ASSERT_EQ(both.size(), 1u);
  EXPECT_EQ(both[0]->id, "T003");
  EXPECT_TRUE(service.search("blockchain").empty());
  EXPECT_TRUE(service.search("   ").empty());
}

TEST_F(TitleServiceTest, ListCanFilterToAvailableTitles) {
  db.titles[0].studentId = "2400001";
  EXPECT_EQ(service.list().size(), 5u);
  auto available = service.list(true);
  EXPECT_EQ(available.size(), 4u);
  for (const Title* t : available) EXPECT_FALSE(t->taken());
}
