#pragma once
// Shared fixtures for the test suite.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

#include "fyp/models.hpp"
#include "fyp/storage.hpp"

namespace fyp::support {

// In-memory stand-in for FileStorage that counts how often it is saved.
class MemoryStorage : public Storage {
 public:
  Database load(Warnings&) override { return stored; }
  void save(const Database& db) override {
    stored = db;
    ++saveCount;
  }

  Database stored;
  int saveCount = 0;
};

// Five titles and three students, nothing confirmed or graded yet.
inline Database sampleDb() {
  Database db;
  db.titles = {
      {"T001", "IoT Based Smart Parking System", ""},
      {"T002", "Mobile Asset Management Application", ""},
      {"T003", "Web Application for Person Counting", ""},
      {"T004", "Stock Screening and Recommendation System", ""},
      {"T005", "Gamified Mood and Wellness Journal", ""},
  };
  db.students = {{"2400001", "Alice Tan"}, {"2400002", "Bob Lim"}, {"2400003", "Chloe Wong"}};
  return db;
}

// A unique directory under the system temp dir, deleted when the object goes away.
class TempDir {
 public:
  TempDir() {
    static std::atomic<int> counter{0};
    auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() / ("fyp_test_" + std::to_string(ticks) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace fyp::support
