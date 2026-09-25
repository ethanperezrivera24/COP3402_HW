COP 3402 - Systems Software
HW2 test bundle for the PL/0 lexical analyzer

WHAT IS IN HERE
---------------
  test_lex.sh                   the autograder script
  header_comment.txt            the header block required by the handout
  test_cases/cases.txt          the list of test cases the script runs,
                                with what each one is worth
  test_cases/purposes.txt       one line per test case saying which scenario
                                that case exists to cover
  test_cases/inputs/            the PL/0 programs the test cases scan
  expected_results/             the correct output for every test case, and
                                the correct tokens.txt and nametable.txt

HOW TO USE IT
-------------
  On Eustis:

    unzip lex_test.zip
    cd lex_test
    # get your lex.c into this folder, however you like
    ls lex.c           # confirms it is here
    chmod +x test_lex.sh
    ./test_lex.sh lex.c

  1. Unzipping creates a folder named lex_test. Everything lives in it.
  2. Your lex.c has to end up DIRECTLY INSIDE that folder, next to
     test_lex.sh. How it gets there is up to you: cp it, scp it, drag and
     drop it, or write it there in an editor. Nothing depends on the
     method. What matters is that lex.c is in the folder, and "ls lex.c"
     is how you confirm it.

     The file must be named lex.c. Exactly that, in lower case. The
     check is case sensitive, so LEX.c and lex.C are rejected along with
     main.c, mylex.c and every other name. A submission under any other
     name scores 0 even when the program is correct, so the script
     refuses to run rather than let you find out after the deadline.
  3. chmod +x test_lex.sh makes the script executable. You only do this
     once per unzipped copy.
  4. Run it from inside lex_test, as often as you like. The script looks
     for test_cases/ and expected_results/ in the directory you run it
     from, so leave test_lex.sh where it is.

WHAT THE SCRIPT DOES
--------------------
  It compiles your program with the course flags, runs all one hundred
  eighty test cases, and for each one checks three things: the exit status,
  the output your program printed, and, on the cases that carry file points,
  the two files your program wrote.

  The two streams are separate. Standard output is worth 90 points and the
  two written files are worth 10, for 100. You can earn a case's output
  points and lose its file points, or the reverse, so the report prints
  both figures before it prints your score.

  It then checks the one code requirement no test can see: an Author(s):
  line that is no longer the placeholder. That is an automatic zero in the
  handout, so the script prints a FINAL SCORE that takes it into account.
  That final score is what the grader records.

  If Webcourses renamed your file to lex-1.c on a resubmission, the script
  renames it back to lex.c and carries on. That is the only name besides
  lex.c it will accept.

  Your scanner writes tokens.txt and nametable.txt into the folder it runs
  in, so the script deletes both before each case and compares them after.
  Do not be surprised to see those two files appear and change while it
  runs; that is your own program writing them.

WHY THERE ARE SO MANY CASES
---------------------------
  This is a test suite for a piece of software, not a list of exercises.
  Every case is here because it covers one scenario the scanner has to
  handle, and test_cases/purposes.txt says in one line what that scenario
  is. No two cases cover the same one.

  Read purposes.txt straight through before you start. It is the shortest
  complete description of the behavior you are building: what a comment
  that never closes looks like, which half-formed delimiters do not count,
  where whitespace matters and where it does not, which bytes are errors,
  and what a position has to be counted from.

  When a case fails, the script prints that line beside the failure, so the
  report tells you what was being checked and not only that your output did
  not match. The same line is the second line of
  failed_cases/<case>/info.txt.

  The cases are not worth the same amount. The operators carry the most per
  case, because choosing between a one-character and a two-character token
  is the decision this homework is about, and a scanner that chooses wrong
  produces a token stream that is wrong from that point to the end of the
  file. The lexical errors carry the largest total, because there are eleven
  of them and each one is a different behavior. Every point value is in
  test_cases/cases.txt, in the open, before you write a line.

  Sixty cases contain a lexical error on purpose. Each one must print
  everything scanned before the error, then the error with its line and
  column, then exit with a non-zero status. Five more cases run the program
  with the wrong number of arguments or a file that cannot be opened; those
  compare the message text, which the handout gives exactly.

WHERE TO LOOK WHEN A CASE FAILS
-------------------------------
  student_output/<case>_output.txt       exactly what your program printed
  failed_cases/<case>/info.txt           what the case covers, and the
                                         command that was run
  failed_cases/<case>/diff.txt           your output on the left, expected on the right
  failed_cases/<case>/tokens.txt.diff    your tokens.txt against the expected one
  failed_cases/<case>/nametable.txt.diff your nametable.txt against the expected one

  The last two appear only when a case lost its file points.

  Both directories are deleted and rebuilt every time the script runs.

  Lines beginning with "<" are yours. Lines beginning with ">" are expected.

NOTES
-----
  Standard output and standard error are captured together, so it does not
  matter which stream you print your error and usage messages to.

  Comparison ignores blank lines and differences in spaces and tabs. Your
  columns do not have to line up with the reference's; the values in them
  do, and so does their order. This applies to the two written files as
  well.

  Some input files hold bytes you cannot see: carriage returns, a control
  byte, a zero byte, and multi-byte characters a word processor produces.
  They are meant to be there. Open one with "od -c <file>" rather than in
  an editor if what you see does not explain what your scanner did.

  You may run this script as many times as you want. Nothing is recorded
  and nothing is submitted from here. Do not modify the files in
  test_cases/ or expected_results/; the instructor grades against the
  original copies, and a case you edited into passing still fails there.
