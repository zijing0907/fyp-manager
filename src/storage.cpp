#include "fyp/storage.hpp"

#include <fstream>
#include <functional>
#include <stdexcept>
#include <system_error>

namespace fyp {

namespace {

void writeAtomically(const std::filesystem::path& target, const std::function<void(std::ostream&)>& write) {
  std::filesystem::path temp = target;
  temp += ".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot write " + temp.string());
    write(out);
    out.flush();
    if (!out) throw std::runtime_error("Failed while writing " + temp.string() + " (disk full?)");
  }
  std::error_code ec;
  std::filesystem::rename(temp, target, ec);
  if (ec) throw std::runtime_error("Cannot replace " + target.string() + ": " + ec.message());
}

// Opens `name` inside `dir` and hands it to `reader`. Returns false if the file does not exist.
bool readFile(const std::filesystem::path& dir, const std::string& name,
              const std::function<void(std::istream&)>& reader) {
  std::ifstream in(dir / name, std::ios::binary);
  if (!in) return false;
  reader(in);
  return true;
}

}  // namespace

FileStorage::FileStorage(std::filesystem::path directory) : directory_(std::move(directory)) {}

Database FileStorage::load(Warnings& warnings) {
  Database db;
  if (!readFile(directory_, "titles.txt", [&](std::istream& in) { readTitles(in, "titles.txt", db, warnings); })) {
    warnings.push_back("titles.txt not found in " + directory_.string() + "; starting with no titles");
  }
  readFile(directory_, "students.txt", [&](std::istream& in) { readStudents(in, "students.txt", db, warnings); });
  readFile(directory_, "shortlists.txt", [&](std::istream& in) { readShortlists(in, "shortlists.txt", db, warnings); });
  readFile(directory_, "grades.txt", [&](std::istream& in) { readMarks(in, "grades.txt", db, warnings); });
  repairReferences(db, warnings);
  return db;
}

void FileStorage::save(const Database& db) {
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) throw std::runtime_error("Cannot create " + directory_.string() + ": " + ec.message());

  writeAtomically(directory_ / "titles.txt", [&](std::ostream& out) { writeTitles(out, db); });
  writeAtomically(directory_ / "students.txt", [&](std::ostream& out) { writeStudents(out, db); });
  writeAtomically(directory_ / "shortlists.txt", [&](std::ostream& out) { writeShortlists(out, db); });
  writeAtomically(directory_ / "grades.txt", [&](std::ostream& out) { writeMarks(out, db); });
}

}  // namespace fyp
