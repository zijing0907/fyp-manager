#pragma once
// Interactive text UI. Input and output streams are injected so that whole
// sessions can be scripted in tests, and reading stops cleanly at end-of-input.

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "fyp/grading_service.hpp"
#include "fyp/registration_service.hpp"
#include "fyp/style.hpp"
#include "fyp/title_service.hpp"

namespace fyp {

class Cli {
 public:
  struct Options {
    bool echoInput = false;  // repeat each input line in the output; makes piped transcripts readable
    Style style;             // plain by default; main() turns colour on for interactive terminals
    // Wait for Enter after each action so its output is read before the menu is
    // printed again. main() turns this on only when a person is typing.
    bool pauseAfterActions = false;
  };

  Cli(Database& db, Storage& storage, std::istream& in, std::ostream& out, GradingService::Clock clock,
      Options options);

  // Runs until the user exits or the input stream ends.
  void run();

 private:
  struct InputClosed {};  // thrown when stdin reaches end-of-file

  struct MenuItem {
    std::string key;
    std::string label;
  };
  struct MenuSection {
    std::string name;
    std::vector<MenuItem> items;
  };

  // --- input helpers ---
  std::string ask(const std::string& prompt);
  static bool isCancel(const std::string& input);
  void report(const Status& status);
  const Student* askStudent();
  std::optional<std::string> askTitleId(const std::string& prompt);
  std::optional<int> askOption(const std::string& prompt, int max);
  std::optional<double> askMark(const std::string& criterion);
  bool confirmYes(const std::string& prompt);
  void pause();

  // --- layout helpers ---
  void say(const std::string& text, Tone tone = Tone::Plain);  // one indented line
  void printBanner(const std::string& title, const std::string& subtitle = "");
  void printSection(const std::string& name);
  void printMenu(const std::vector<MenuSection>& sections, const std::string& exitLabel);
  void printField(const std::string& label, const std::string& value, Tone tone, const std::string& note = "");
  void printConfirmedTitle(const Title& title);

  // --- output helpers ---
  void printTitles(const std::vector<const Title*>& titles, bool showHolder);
  void printResult(const StudentResult& result);

  // --- menus ---
  void lecturerMenu();
  void studentLogin();
  void studentMenu(const std::string& studentId);

  // --- shared actions ---
  void listTitles(bool showHolder);  // showHolder: lecturers also see who holds a taken title
  void searchTitles(bool showHolder);

  // --- lecturer actions ---
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
  Style style_;
  TitleService titles_;
  RegistrationService registration_;
  GradingService grading_;
};

}  // namespace fyp
