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
