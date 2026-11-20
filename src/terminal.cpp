#include "fyp/terminal.hpp"

#include <cstdio>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

namespace fyp::terminal {

bool shouldUseColor(ColorMode mode, bool isTerminal, const char* noColor, const char* term) {
  switch (mode) {
    case ColorMode::Always: return true;
    case ColorMode::Never: return false;
    case ColorMode::Auto: break;
  }
  if (!isTerminal) return false;
  if (noColor && *noColor) return false;
  if (term && std::string_view(term) == "dumb") return false;
  return true;
}

#ifdef _WIN32

bool stdinIsTerminal() { return _isatty(_fileno(stdin)) != 0; }
bool stdoutIsTerminal() { return _isatty(_fileno(stdout)) != 0; }

bool enableColorOutput() {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode)) return false;
  if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return false;
  SetConsoleOutputCP(CP_UTF8);  // so the block and arrow glyphs are not shown as '?'
  return true;
}

#else

bool stdinIsTerminal() { return isatty(STDIN_FILENO) != 0; }
bool stdoutIsTerminal() { return isatty(STDOUT_FILENO) != 0; }

bool enableColorOutput() { return true; }

#endif

}  // namespace fyp::terminal
