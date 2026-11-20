#include <gtest/gtest.h>

#include "fyp/style.hpp"
#include "fyp/terminal.hpp"

using namespace fyp;
using fyp::terminal::ColorMode;
using fyp::terminal::shouldUseColor;

TEST(Style, PlainStyleLeavesTextUnchanged) {
  Style plain = Style::plain();
  EXPECT_EQ(plain.paint("Distinction", Tone::Good), "Distinction");
  EXPECT_EQ(plain.badge("Pass", Tone::Info), " Pass ");
  EXPECT_STREQ(plain.barBlock(), "#");
  EXPECT_STREQ(plain.arrow(), "->");
}

TEST(Style, ColourWrapsTextInEscapeCodesAndResets) {
  Style colour(true, false);
  EXPECT_EQ(colour.paint("Fail", Tone::Bad), "\x1b[31mFail\x1b[0m");
  EXPECT_EQ(colour.paint("[1]", Tone::Key), "\x1b[38;5;214m[1]\x1b[0m");
  EXPECT_EQ(colour.badge("Pass", Tone::Info), "\x1b[1;7;36m Pass \x1b[0m");
  EXPECT_EQ(colour.paint("as is", Tone::Plain), "as is") << "Plain never adds codes";
  EXPECT_EQ(colour.paint("", Tone::Bad), "") << "no codes around nothing";
  EXPECT_STREQ(colour.barBlock(), "#") << "glyphs follow the unicode flag, not the colour flag";
}

TEST(Style, FancyStyleUsesUnicodeGlyphs) {
  Style fancy = Style::fancy();
  EXPECT_TRUE(fancy.color());
  EXPECT_STREQ(fancy.barBlock(), "\xE2\x96\x88");
  EXPECT_STREQ(fancy.arrow(), "\xE2\x86\x92");
}

TEST(Style, StripAnsiRemovesEveryEscapeSequence) {
  Style colour(true, true);
  std::string text = colour.paint("Final", Tone::Label) + ": " + colour.paint("85.91", Tone::Strong) + " " +
                     colour.badge("Distinction", Tone::Good) + colour.paint("!", Tone::Key);
  EXPECT_EQ(stripAnsi(text), "Final: 85.91  Distinction !");
  EXPECT_EQ(stripAnsi("no codes here"), "no codes here");
  EXPECT_EQ(stripAnsi("cut off \x1b[3"), "cut off ") << "a truncated sequence is dropped, not printed";
}

TEST(Terminal, AutoColoursOnlyAnInteractiveTerminal) {
  EXPECT_TRUE(shouldUseColor(ColorMode::Auto, true, nullptr, "xterm-256color"));
  EXPECT_FALSE(shouldUseColor(ColorMode::Auto, false, nullptr, "xterm-256color")) << "piped to a file or another program";
}

TEST(Terminal, AutoRespectsNoColorAndDumbTerminals) {
  EXPECT_FALSE(shouldUseColor(ColorMode::Auto, true, "1", "xterm"));
  EXPECT_TRUE(shouldUseColor(ColorMode::Auto, true, "", "xterm")) << "NO_COLOR only counts when non-empty";
  EXPECT_FALSE(shouldUseColor(ColorMode::Auto, true, nullptr, "dumb"));
  EXPECT_TRUE(shouldUseColor(ColorMode::Auto, true, nullptr, nullptr)) << "TERM is often unset on Windows";
}

TEST(Terminal, ExplicitFlagsOverrideDetection) {
  EXPECT_TRUE(shouldUseColor(ColorMode::Always, false, "1", "dumb"));
  EXPECT_FALSE(shouldUseColor(ColorMode::Never, true, nullptr, "xterm-256color"));
}
