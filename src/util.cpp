#include "fyp/util.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <exception>
#include <iomanip>
#include <sstream>

namespace fyp::util {

std::string trim(std::string_view s) {
  std::size_t begin = 0;
  std::size_t end = s.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return std::string(s.substr(begin, end - begin));
}

std::string toLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::vector<std::string> splitWhitespace(std::string_view s) {
  std::vector<std::string> words;
  std::size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    std::size_t start = i;
    while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i > start) words.emplace_back(s.substr(start, i - start));
  }
  return words;
}

namespace {

bool isContinuationByte(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

// Byte offset at which column `columns` starts (or s.size() if the text is shorter).
std::size_t byteOffsetOfColumn(std::string_view s, std::size_t columns) {
  std::size_t seen = 0;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (isContinuationByte(s[i])) continue;
    if (seen == columns) return i;
    ++seen;
  }
  return s.size();
}

}  // namespace

std::size_t displayWidth(std::string_view s) {
  return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), [](char c) { return !isContinuationByte(c); }));
}

std::string padRight(std::string_view s, std::size_t width) {
  std::size_t w = displayWidth(s);
  return std::string(s) + std::string(w < width ? width - w : 0, ' ');
}

std::string padLeft(std::string_view s, std::size_t width) {
  std::size_t w = displayWidth(s);
  return std::string(w < width ? width - w : 0, ' ') + std::string(s);
}

std::string repeat(std::string_view s, std::size_t times) {
  std::string out;
  out.reserve(s.size() * times);
  for (std::size_t i = 0; i < times; ++i) out += s;
  return out;
}

std::vector<std::string> wrapText(std::string_view text, std::size_t width) {
  if (width == 0) width = 1;
  std::vector<std::string> lines;
  std::string current;
  std::size_t currentWidth = 0;
  for (std::string word : splitWhitespace(text)) {
    std::size_t w = displayWidth(word);
    while (w > width) {  // a word longer than a whole line goes on lines of its own, split
      if (!current.empty()) {
        lines.push_back(current);
        current.clear();
        currentWidth = 0;
      }
      std::size_t cut = byteOffsetOfColumn(word, width);
      lines.push_back(word.substr(0, cut));
      word.erase(0, cut);
      w = displayWidth(word);
    }
    if (word.empty()) continue;
    if (current.empty()) {
      current = word;
      currentWidth = w;
    } else if (currentWidth + 1 + w <= width) {
      current += ' ';
      current += word;
      currentWidth += 1 + w;
    } else {
      lines.push_back(current);
      current = word;
      currentWidth = w;
    }
  }
  if (!current.empty() || lines.empty()) lines.push_back(current);
  return lines;
}

std::string escapeField(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '|':  out += "\\|";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      default:   out += c;
    }
  }
  return out;
}

std::vector<std::string> splitFields(std::string_view line) {
  std::vector<std::string> fields(1);
  for (std::size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '|') {
      fields.emplace_back();
    } else if (c == '\\' && i + 1 < line.size()) {
      char next = line[++i];
      switch (next) {
        case 'n': fields.back() += '\n'; break;
        case 'r': fields.back() += '\r'; break;
        case '\\':
        case '|': fields.back() += next; break;
        default:  fields.back() += '\\'; fields.back() += next;  // unknown escape: keep as written
      }
    } else {
      fields.back() += c;
    }
  }
  return fields;
}

std::string joinFields(const std::vector<std::string>& fields) {
  std::string out;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) out += '|';
    out += escapeField(fields[i]);
  }
  return out;
}

std::vector<std::string> splitList(std::string_view s, char sep) {
  std::vector<std::string> items;
  if (s.empty()) return items;
  std::size_t start = 0;
  while (true) {
    std::size_t pos = s.find(sep, start);
    items.emplace_back(s.substr(start, pos == std::string_view::npos ? pos : pos - start));
    if (pos == std::string_view::npos) break;
    start = pos + 1;
  }
  return items;
}

bool isAllDigits(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

bool parseDecimal(std::string_view s, double& out) {
  bool sawDot = false;
  bool sawDigit = false;
  for (char c : s) {
    if (c == '.') {
      if (sawDot) return false;
      sawDot = true;
    } else if (c >= '0' && c <= '9') {
      sawDigit = true;
    } else {
      return false;
    }
  }
  if (!sawDigit) return false;
  try {
    out = std::stod(std::string(s));
  } catch (const std::exception&) {  // out_of_range for absurdly long digit strings
    return false;
  }
  return true;
}

double round2(double v) { return std::round(v * 100.0) / 100.0; }

std::string formatDouble(double v, int precision) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(precision) << v;
  return os.str();
}

std::string timestampNow() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &local);
  return buf;
}

}  // namespace fyp::util
