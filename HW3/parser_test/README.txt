COP 3402 - Systems Software
HW3 test bundle for the PL/0 parser and symbol table

WHAT IS IN HERE
---------------
  test_parser.sh                the autograder script
  header_comment.txt            the header block required by the handout
  test_cases/cases.txt          the list of test cases the script runs, with
                                what each one is worth and how it is run
  test_cases/purposes.txt       one line per test case saying which scenario
                                that case exists to cover
  test_cases/inputs/            for every case: the PL/0 source program
                                <case>.txt, and the tokens.txt and
                                nametable.txt a correct HW2 scanner writes for
                                it, as <case>.tokens.txt and
                                <case>.nametable.txt
  expected_results/             the correct parser output for every test case

HOW TO USE IT
-------------
  On Eustis:

    unzip parser_test.zip
    cd parser_test
    # get your parser.c and your HW2 lex.c into this folder
    ls parser.c lex.c      # confirms both are here
    chmod +x test_parser.sh
    ./test_parser.sh parser.c lex.c

  1. Unzipping creates a folder named parser_test. Everything lives in it.
  2. Give the script both files, parser.c first and lex.c second. Either
     may be a path, such as ../hw2/lex.c. Without both arguments the script
     prints its usage line and runs nothing. The names are exact and case
     sensitive: parser.c and lex.c. The script refuses any other name for
     parser.c, because a submission under another name scores 0. A lex.c
     that is missing, misnamed, or does not compile costs only the pipeline
     cases.
  3. chmod +x test_parser.sh makes the script executable. You do this once.
  4. Run it from inside parser_test, as often as you like. Add --quiet to
     print the summary only.

WHAT THE SCRIPT DOES
--------------------
  It compiles parser.c and lex.c with the course flags and runs every test
  case, each in its own empty folder:

    - most cases copy the posted <case>.tokens.txt and <case>.nametable.txt
      in as tokens.txt and nametable.txt, then run ./parser. Your parser is
      graded on its own: a mistake in your HW2 scanner cannot cost these.
    - the pipeline cases (names beginning with p) copy only the source
      program in, run YOUR ./lex <case>.txt, then ./parser. These are the
      cases that need your HW2 scanner to be right.

  A case earns its points only when the exit status is right AND the
  parser's output matches expected_results/<case>.txt. Blank lines and
  spacing inside a line are ignored; the text and its order are not.

  The whole run is limited to 15 minutes. A case still running at that
  moment is stopped and fails, and the cases after it are not run. Those
  cases score 0; the cases already finished keep their points.

  It then checks one code requirement that no case can see: an Author(s):
  line in parser.c that is no longer the placeholder. The FINAL SCORE it
  prints takes that into account. The instructor checks the rest of the
  header by reading parser.c: every labeled line of header_comment.txt
  must be there, so compare your header with that file before you submit.

WHEN A CASE FAILS
-----------------
  student_output/<case>_output.txt          what your parser printed
  student_output/<case>_lex_output.txt      what your lex printed (pipeline)
  failed_cases/<case>/info.txt              what the case covers and why it
                                            failed
  failed_cases/<case>/diff.txt              your output against the expected
                                            output; lines beginning with < are
                                            yours, lines beginning with > are
                                            expected

  The first line of diff.txt is where your output first departs from the
  expected output. The cause is on that line or earlier, because a check
  your parser skips prints nothing, so read the trace lines above it too.

CHECKING YOUR HW2 SCANNER AGAINST THE POSTED FILES
--------------------------------------------------
  Every case posts the token files a correct scanner writes. To see whether
  your lex agrees, for any case:

    mkdir check && cd check
    cp ../test_cases/inputs/a17_hw2_spine_program.txt .
    ../lex a17_hw2_spine_program.txt > /dev/null
    diff tokens.txt ../test_cases/inputs/a17_hw2_spine_program.tokens.txt
    diff nametable.txt ../test_cases/inputs/a17_hw2_spine_program.nametable.txt
    cd .. && rm -r check

  (Build ../lex first with: gcc -Wall -Wextra -std=c11 -O2 lex.c -o lex)
  No output from both diff commands means your scanner wrote exactly the
  posted files.
