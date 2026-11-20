#pragma once
// Deciding whether the program's output can be coloured. Only main() touches
// the real terminal; the decision itself is a pure function so it can be tested.

namespace fyp::terminal {

enum class ColorMode { Auto, Always, Never };

// Auto colours only an interactive terminal, and respects the NO_COLOR
// convention (https://no-color.org: any non-empty value turns colour off) and
// TERM=dumb. `noColor` and `term` are the environment values, or nullptr if unset.
bool shouldUseColor(ColorMode mode, bool stdoutIsTerminal, const char* noColor, const char* term);

bool stdinIsTerminal();
bool stdoutIsTerminal();

// Prepares the console for ANSI colour and UTF-8 output. Needed on Windows,
// where it turns on virtual-terminal processing; a no-op elsewhere. Returns
// false if the console refuses.
bool enableColorOutput();

}  // namespace fyp::terminal
