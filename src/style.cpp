#include "fyp/style.hpp"

namespace fyp {

namespace {

// SGR parameters for each tone. The basic 16 colours follow the user's terminal
// theme, so they stay readable on light and dark backgrounds; only Key uses a
// 256-colour orange.
const char* sgr(Tone tone) {
  switch (tone) {
    case Tone::Plain: return "";
    case Tone::Heading: return "1;36";
    case Tone::Label: return "36";
    case Tone::Key: return "38;5;214";
    case Tone::Strong: return "1";
    case Tone::Muted: return "90";
    case Tone::Good: return "32";
    case Tone::Info: return "36";
    case Tone::Warn: return "33";
    case Tone::Bad: return "31";
  }
  return "";
}

std::string wrap(std::string_view text, const std::string& params) {
  return "\x1b[" + params + "m" + std::string(text) + "\x1b[0m";
}

}  // namespace

std::string Style::paint(std::string_view text, Tone tone) const {
  const std::string params = sgr(tone);
  if (!color_ || params.empty() || text.empty()) return std::string(text);
  return wrap(text, params);
}

std::string Style::badge(std::string_view text, Tone tone) const {
  const std::string padded = " " + std::string(text) + " ";
  if (!color_) return padded;
  const std::string params = sgr(tone);
  return wrap(padded, params.empty() ? "1;7" : "1;7;" + params);
}

std::string stripAnsi(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
      // CSI sequence: parameters and intermediates, then one final byte in @..~
      i += 2;
      while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) ++i;
      continue;
    }
    out += s[i];
  }
  return out;
}

}  // namespace fyp
