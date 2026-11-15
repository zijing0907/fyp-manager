#pragma once
// Persistence boundary. Services depend on the abstract Storage, so the text
// files could be swapped for SQLite (or an in-memory fake in tests) without
// touching any business logic.

#include <filesystem>

#include "fyp/models.hpp"
#include "fyp/serialization.hpp"

namespace fyp {

class Storage {
 public:
  virtual ~Storage() = default;
  virtual Database load(Warnings& warnings) = 0;
  // Throws std::runtime_error if the data cannot be written.
  virtual void save(const Database& db) = 0;
};

// Stores the database as four .txt files inside one directory. Missing files
// are treated as empty. Saves go through a temp file + rename, so a crash or a
// full disk mid-write leaves the previous version intact instead of a
// half-written file.
class FileStorage : public Storage {
 public:
  explicit FileStorage(std::filesystem::path directory);

  Database load(Warnings& warnings) override;
  void save(const Database& db) override;

  const std::filesystem::path& directory() const { return directory_; }

 private:
  std::filesystem::path directory_;
};

}  // namespace fyp
