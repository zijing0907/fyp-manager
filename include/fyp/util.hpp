#pragma once
// Small string helpers shared by the storage layer, services and CLI.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fyp::util {

std::string trim(std::string_view s);
std::string toLower(std::string_view s);
std::vector<std::string> splitWhitespace(std::string_view s);

// --- text layout -------------------------------------------------------------
// Widths are counted in UTF-8 code points, so a glyph such as the block used
// in bar charts counts as one column. (Wide CJK characters also count as one,
// which is close enough for these tables.) Never pass text that already holds
// ANSI escape codes: pad first, then paint.

std::size_t displayWidth(std::string_view s);
std::string padRight(std::string_view s, std::size_t width);
std::string padLeft(std::string_view s, std::size_t width);
std::string repeat(std::string_view s, std::size_t times);
// Word-wraps `text` into lines of at most `width` columns, splitting words that
// are longer than a whole line. Always returns at least one (possibly empty) line.
std::vector<std::string> wrapText(std::string_view text, std::size_t width);

// --- '|'-delimited record format -------------------------------------------
// Data files use one record per line with '|' between fields. Free text (such
// as a title) may itself contain '|', so fields are escaped: \\ \| \n \r.

std::string escapeField(std::string_view s);
std::vector<std::string> splitFields(std::string_view line);  // splits on unescaped '|', then unescapes
std::string joinFields(const std::vector<std::string>& fields);
std::vector<std::string> splitList(std::string_view s, char sep = ',');

// --- number and time helpers ----------------------------------------------

bool isAllDigits(std::string_view s);
// Accepts only plain non-negative decimals such as "85", "85.5" or ".5".
// Rejects signs, exponents, "nan"/"inf" and trailing junk, unlike std::stod.
bool parseDecimal(std::string_view s, double& out);
double round2(double v);
std::string formatDouble(double v, int precision = 2);
std::string timestampNow();  // "YYYY-MM-DD HH:MM:SS" in local time

}  // namespace fyp::util
