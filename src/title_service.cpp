#include "fyp/title_service.hpp"

#include <algorithm>

#include "fyp/util.hpp"

namespace fyp {

Status TitleService::validateText(const std::string& text, const std::string& ignoreId) const {
  if (text.size() < kMinLength) return Status::failure("Title must be at least " + std::to_string(kMinLength) + " characters.");
  if (text.size() > kMaxLength) return Status::failure("Title must be at most " + std::to_string(kMaxLength) + " characters.");
  std::string lowered = util::toLower(text);
  for (const Title& t : db_.titles) {
    if (t.id != ignoreId && util::toLower(t.text) == lowered) {
      return Status::failure("That title already exists (" + t.id + ").");
    }
  }
  return Status::success();
}

Status TitleService::add(const std::string& rawText) {
  std::string text = util::trim(rawText);
  if (Status s = validateText(text, ""); !s) return s;

  int next = 1;
  for (const Title& t : db_.titles) next = std::max(next, std::stoi(t.id.substr(1)) + 1);
  std::string id = makeTitleId(next);
  db_.titles.push_back({id, text, ""});
  commit();
  return Status::success("Added " + id + ".");
}

Status TitleService::modify(const std::string& id, const std::string& rawText) {
  Title* title = db_.findTitle(id);
  if (!title) return Status::failure("Title " + id + " not found.");
  std::string text = util::trim(rawText);
  if (Status s = validateText(text, id); !s) return s;

  title->text = text;
  commit();
  return Status::success("Updated " + id + ".");
}

Status TitleService::remove(const std::string& id) {
  Title* title = db_.findTitle(id);
  if (!title) return Status::failure("Title " + id + " not found.");
  if (title->taken()) return Status::failure("Cannot delete " + id + ": it is confirmed for student " + title->studentId + ".");

  // Shortlists reference titles by stable id, so removal never disturbs other entries.
  for (auto& [studentId, picks] : db_.shortlists) {
    picks.erase(std::remove(picks.begin(), picks.end(), id), picks.end());
  }
  db_.titles.erase(db_.titles.begin() + (title - db_.titles.data()));
  commit();
  return Status::success("Deleted " + id + ".");
}

std::vector<const Title*> TitleService::search(const std::string& keywords) const {
  std::vector<std::string> words = util::splitWhitespace(util::toLower(keywords));
  std::vector<const Title*> matches;
  if (words.empty()) return matches;
  for (const Title& t : db_.titles) {
    std::string lowered = util::toLower(t.text);
    bool all = std::all_of(words.begin(), words.end(), [&](const std::string& w) { return lowered.find(w) != std::string::npos; });
    if (all) matches.push_back(&t);
  }
  return matches;
}

std::vector<const Title*> TitleService::list(bool availableOnly) const {
  std::vector<const Title*> result;
  for (const Title& t : db_.titles) {
    if (!availableOnly || !t.taken()) result.push_back(&t);
  }
  return result;
}

}  // namespace fyp
