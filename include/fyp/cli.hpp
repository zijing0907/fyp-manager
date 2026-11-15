#pragma once
// Interactive text UI. Input and output streams are injected so that whole
// sessions can be scripted in tests, and reading stops cleanly at end-of-input.

#include <iosfwd>
#include <optional>
#include <string>

#include "fyp/grading_service.hpp"
#include "fyp/registration_service.hpp"
#include "fyp/title_service.hpp"

namespace fyp {

class Cli {
 public:
  struct Options {
    bool echoInput = false;  // repeat each input line in the output; makes piped transcripts readable
  };

  Cli(Database& db, Storage& storage, std::istream& in, std::ostream& out, GradingService::Clock clock,
      Options options);

  // Runs until the user exits or the input stream ends.
  void run();

 private:
  struct InputClosed {};  // thrown when stdin reaches end-of-file

  // --- input helpers ---
  std::string ask(const std::string& prompt);
  static bool isCancel(const std::string& input);
  void report(const Status& status);
  const Student* askStudent();
  std::optional<std::string> askTitleId(const std::string& prompt);
  std::optional<int> askOption(const std::string& prompt, int max);
  std::optional<double> askMark(const std::string& criterion);
  bool confirmYes(const std::string& prompt);

  // --- output helpers ---
  void printTitles(const std::vector<const Title*>& titles, bool showHolder);
  void printResult(const StudentResult& result);

  // --- menus ---
  void lecturerMenu();
  void studentLogin();
  void studentMenu(const std::string& studentId);

  // --- lecturer actions ---
  void searchTitles();
  void addTitle();
  void modifyTitle();
  void deleteTitle();
  void registerStudent();
  void listStudents();
  void confirmTitle();
  void enterMarks();
  void showStudentResult();
  void showClassSummary();

  // --- student actions ---
  void addToShortlist(const std::string& studentId);
  void removeFromShortlist(const std::string& studentId);
  void showShortlist(const std::string& studentId);

  Database& db_;
  std::istream& in_;
  std::ostream& out_;
  Options options_;
  TitleService titles_;
  RegistrationService registration_;
  GradingService grading_;
};

}  // namespace fyp
