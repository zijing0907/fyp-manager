#include <gtest/gtest.h>

#include "fyp/util.hpp"

using namespace fyp::util;

TEST(Util, TrimRemovesSurroundingWhitespaceOnly) {
  EXPECT_EQ(trim("  hello world \t\r\n"), "hello world");
  EXPECT_EQ(trim(""), "");
  EXPECT_EQ(trim("   "), "");
}

TEST(Util, ToLowerAndSplitWhitespace) {
  EXPECT_EQ(toLower("Mobile-BASED App"), "mobile-based app");
  EXPECT_EQ(splitWhitespace("  a  bb\tccc \n"), (std::vector<std::string>{"a", "bb", "ccc"}));
  EXPECT_TRUE(splitWhitespace("   ").empty());
}

TEST(Util, EscapeAndSplitRoundTripSpecialCharacters) {
  std::vector<std::string> fields = {"plain", "has|pipe", "back\\slash", "line\nbreak\r", "", "trailing\\"};
  std::string line = joinFields(fields);
  EXPECT_EQ(line.find('\n'), std::string::npos) << "an encoded record must stay on one line";
  EXPECT_EQ(splitFields(line), fields);
}

TEST(Util, SplitFieldsKeepsEmptyFieldsAndHonoursEscapedPipes) {
  EXPECT_EQ(splitFields("a||b"), (std::vector<std::string>{"a", "", "b"}));
  EXPECT_EQ(splitFields("a|"), (std::vector<std::string>{"a", ""}));
  EXPECT_EQ(splitFields("a\\|b|c"), (std::vector<std::string>{"a|b", "c"}));
  EXPECT_EQ(splitFields("unknown\\xescape"), (std::vector<std::string>{"unknown\\xescape"}));
}

TEST(Util, SplitList) {
  EXPECT_EQ(splitList("T001,T002,T003"), (std::vector<std::string>{"T001", "T002", "T003"}));
  EXPECT_EQ(splitList("single"), (std::vector<std::string>{"single"}));
  EXPECT_TRUE(splitList("").empty());
}

TEST(Util, ParseDecimalAcceptsPlainNonNegativeNumbers) {
  double v = -1;
  for (const char* good : {"85", "85.5", "0", "100", ".5", "5.", "007"}) {
    EXPECT_TRUE(parseDecimal(good, v)) << good;
  }
  ASSERT_TRUE(parseDecimal("85.5", v));
  EXPECT_DOUBLE_EQ(v, 85.5);
}

TEST(Util, ParseDecimalRejectsEverythingElse) {
  double v = 0;
  for (const char* bad : {"", ".", "-1", "+5", "1e3", "nan", "inf", "0x10", "12abc", "1.2.3", " 5", "5 ", "१२"}) {
    EXPECT_FALSE(parseDecimal(bad, v)) << '"' << bad << '"';
  }
  EXPECT_FALSE(parseDecimal(std::string(400, '9'), v)) << "overflow must be rejected, not thrown";
}

TEST(Util, IsAllDigitsAndRounding) {
  EXPECT_TRUE(isAllDigits("2400001"));
  EXPECT_FALSE(isAllDigits(""));
  EXPECT_FALSE(isAllDigits("24O0001"));
  EXPECT_DOUBLE_EQ(round2(74.994), 74.99);
  EXPECT_DOUBLE_EQ(round2(74.996), 75.0);
  EXPECT_EQ(formatDouble(3.14159), "3.14");
  EXPECT_EQ(formatDouble(5.0, 0), "5");
}

TEST(Util, TimestampHasExpectedShape) {
  std::string ts = timestampNow();
  ASSERT_EQ(ts.size(), 19u);
  EXPECT_EQ(ts[4], '-');
  EXPECT_EQ(ts[10], ' ');
  EXPECT_EQ(ts[13], ':');
}
