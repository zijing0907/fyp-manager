#pragma once
// How the CLI looks: ANSI colours and a few Unicode glyphs, both optional.
//
// The CLI says what a piece of text *means* (a Tone); the Style decides how
// that is drawn. With colour off, the output is plain ASCII, so piped
// sessions, log files and the tests see exactly the same text without escape
// codes.

#include <string>
#include <string_view>

namespace fyp {

enum class Tone {
  Plain,
  Heading,  // banner and section titles
  Label,    // field names, box borders and rules
  Key,      // menu keys and input prompts: the user is expected to type
  Strong,   // values to read first (final mark, counts, table headers)
  Muted,    // secondary information (hints, held-by notes, taken titles)
  Good,     // available, distinction, success messages
  Info,     // pass
  Warn,     // validation problems, assessor gaps, things still pending
  Bad,      // errors, fail, taken
};

class Style {
 public:
  constexpr Style() = default;
  constexpr Style(bool color, bool unicode) : color_(color), unicode_(unicode) {}

  static constexpr Style plain() { return {false, false}; }
  static constexpr Style fancy() { return {true, true}; }  // colour + Unicode glyphs

  bool color() const { return color_; }
  bool unicode() const { return unicode_; }

  // `text` wrapped in the ANSI codes for `tone`, or unchanged when colour is off.
  std::string paint(std::string_view text, Tone tone) const;
  // A short label padded with one space each side; drawn in reverse video
  // (coloured background) when colour is on.
  std::string badge(std::string_view text, Tone tone) const;

  // Glyphs. Each pair has the same display width except arrow().
  const char* barBlock() const { return unicode_ ? "\xE2\x96\x88" : "#"; }  // U+2588 full block
  const char* arrow() const { return unicode_ ? "\xE2\x86\x92" : "->"; }    // U+2192 rightwards arrow
  const char* dot() const { return unicode_ ? "\xC2\xB7" : "-"; }           // U+00B7 middle dot

 private:
  bool color_ = false;
  bool unicode_ = false;
};

// Removes ANSI escape sequences, leaving the text a user would read.
std::string stripAnsi(std::string_view s);

}  // namespace fyp
