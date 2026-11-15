#pragma once

#include <string>
#include <vector>

#include "fyp/service.hpp"

namespace fyp {

// FYP title management: create, search, modify and delete.
class TitleService : public Service {
 public:
  static constexpr std::size_t kMinLength = 5;
  static constexpr std::size_t kMaxLength = 200;

  using Service::Service;

  // On success the message reports the new id, e.g. "Added T031."
  Status add(const std::string& text);
  Status modify(const std::string& id, const std::string& newText);
  // A title that a student has already been confirmed to cannot be deleted.
  // Deleting a title also removes it from every shortlist.
  Status remove(const std::string& id);

  // Every whitespace-separated keyword must appear (case-insensitive) in the title.
  std::vector<const Title*> search(const std::string& keywords) const;
  std::vector<const Title*> list(bool availableOnly = false) const;

 private:
  Status validateText(const std::string& text, const std::string& ignoreId) const;
};

}  // namespace fyp
