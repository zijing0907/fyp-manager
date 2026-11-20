FYP Management System
=====================

A command-line program for managing final-year-project titles, student
shortlists and grading. Source code, design notes and tests:
https://github.com/zijing0907/fyp-manager


Running it
----------

Windows   Double-click fyp_manager.exe, or run it from a terminal in this
          folder:
              .\fyp_manager.exe
          If SmartScreen says "Windows protected your PC", choose
          "More info" and then "Run anyway". The program is not code-signed,
          so Windows asks once.

macOS     Open Terminal in this folder and run:
              ./fyp_manager
          macOS blocks downloaded programs that are not notarised. If it
          refuses to start, run this once in this folder:
              xattr -d com.apple.quarantine fyp_manager
          or right-click fyp_manager in Finder, choose Open, then Open again.

The "data" folder holds a sample data set (30 project titles, 7 fictional
students and their marks). The program saves your changes back into it.


Things to try
-------------

  1  Lecturer, then 11 for the class summary or 10 and 2400004 for a
     student whose two assessors disagree
  2  Student, then log in as 2400006 to browse titles and the shortlist


Options
-------

  --data DIR    use another data folder (a new folder starts empty)
  --no-color    plain text, no colours
  --version     show the version
  --help        list all options
