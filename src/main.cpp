#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "fyp/cli.hpp"
#include "fyp/storage.hpp"
#include "fyp/style.hpp"
#include "fyp/terminal.hpp"
#include "fyp/util.hpp"

namespace {

void printUsage(const char* program) {
  std::cout << "FYP Management System\n\n"
            << "Usage: " << program << " [--data DIR] [--echo] [--color | --no-color]\n\n"
            << "  --data DIR   folder holding titles.txt, students.txt, shortlists.txt and grades.txt\n"
            << "               (default: ./data; missing files are treated as empty)\n"
            << "  --echo       repeat each input line in the output (for scripted demos)\n"
            << "  --color      always use colours, even when the output is not a terminal\n"
            << "  --no-color   never use colours (also: set the NO_COLOR environment variable)\n"
            << "  --help       show this message\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path dataDir = "data";
  fyp::Cli::Options options;
  auto colorMode = fyp::terminal::ColorMode::Auto;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      dataDir = argv[++i];
    } else if (std::strcmp(argv[i], "--echo") == 0) {
      options.echoInput = true;
    } else if (std::strcmp(argv[i], "--color") == 0 || std::strcmp(argv[i], "--colour") == 0) {
      colorMode = fyp::terminal::ColorMode::Always;
    } else if (std::strcmp(argv[i], "--no-color") == 0 || std::strcmp(argv[i], "--no-colour") == 0) {
      colorMode = fyp::terminal::ColorMode::Never;
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n\n";
      printUsage(argv[0]);
      return 2;
    }
  }

  bool color = fyp::terminal::shouldUseColor(colorMode, fyp::terminal::stdoutIsTerminal(), std::getenv("NO_COLOR"),
                                             std::getenv("TERM"));
  // A Windows console that cannot show ANSI codes would print them as junk, so fall
  // back to plain text unless colour was explicitly requested.
  if (color && !fyp::terminal::enableColorOutput() && colorMode != fyp::terminal::ColorMode::Always) color = false;
  options.style = color ? fyp::Style::fancy() : fyp::Style::plain();
  // Pausing only makes sense for a person at a terminal; piped scripts run straight through.
  options.pauseAfterActions = fyp::terminal::stdinIsTerminal() && fyp::terminal::stdoutIsTerminal();

  try {
    fyp::FileStorage storage(dataDir);
    fyp::Warnings warnings;
    fyp::Database db = storage.load(warnings);
    for (const std::string& w : warnings) std::cerr << "warning: " << w << '\n';

    std::cout << options.style.paint("Loaded " + std::to_string(db.titles.size()) + " titles and " +
                                         std::to_string(db.students.size()) + " students from " + dataDir.string() + "/",
                                     fyp::Tone::Muted)
              << '\n';
    fyp::Cli cli(db, storage, std::cin, std::cout, fyp::util::timestampNow, options);
    cli.run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
