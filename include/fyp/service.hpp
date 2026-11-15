#pragma once

#include "fyp/models.hpp"
#include "fyp/storage.hpp"

namespace fyp {

// Common base for the services: they share one in-memory Database and write it
// back through Storage after every successful change.
class Service {
 public:
  Service(Database& db, Storage& storage) : db_(db), storage_(storage) {}

 protected:
  void commit() { storage_.save(db_); }

  Database& db_;
  Storage& storage_;
};

}  // namespace fyp
