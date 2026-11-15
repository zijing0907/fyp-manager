#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "fyp/cli.hpp"
#include "fyp/storage.hpp"
#include "fyp/util.hpp"

namespace {

void printUsage(const char* program) {
  std::cout << "FYP Management System\n\n"
            << "Usage: " << program << " [--data DIR] [--echo]\n\n"
            << "  --data DIR  folder holding titles.txt, students.txt, shortlists.txt and grades.txt\n"
            << "              (default: ./data; missing files are treated as empty)\n"
            << "  --echo      repeat each input line in the output (for scripted demos)\n"
            << "  --help      show this message\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path dataDir = "data";
  fyp::Cli::Options options;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      dataDir = argv[++i];
    } else if (std::strcmp(argv[i], "--echo") == 0) {
      options.echoInput = true;
    } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n\n";
      printUsage(argv[0]);
      return 2;
    }
  }

  try {
    fyp::FileStorage storage(dataDir);
    fyp::Warnings warnings;
    fyp::Database db = storage.load(warnings);
    for (const std::string& w : warnings) std::cerr << "warning: " << w << '\n';

    std::cout << "Loaded " << db.titles.size() << " titles and " << db.students.size() << " students from "
              << dataDir.string() << "/\n";
    fyp::Cli cli(db, storage, std::cin, std::cout, fyp::util::timestampNow, options);
    cli.run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
