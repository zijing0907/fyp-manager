#pragma once
// Small string helpers shared by the storage layer, services and CLI.

#include <string>
#include <string_view>
#include <vector>

namespace fyp::util {

std::string trim(std::string_view s);
std::string toLower(std::string_view s);
std::vector<std::string> splitWhitespace(std::string_view s);

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
