COP 3402 - Systems Software
HW4 test bundle for the PL/0 compiler (parser, symbol table, code generator)

WHAT IS IN HERE
---------------
  test_parsercodegen.sh         the autograder script
  header_comment.txt            the header block required by the handout
  test_cases/cases.txt          the list of test cases the script runs, with
                                what each one is worth and how it is run
  test_cases/purposes.txt       one line per test case saying which scenario
                                that case exists to cover
  test_cases/inputs/            for every case: the PL/0 source program
                                <case>.txt, and the tokens.txt and
                                nametable.txt a correct HW2 scanner writes for
                                it, as <case>.tokens.txt and
                                <case>.nametable.txt; for a pipeline case that
                                reads a number, <case>.stdin
  expected_results/             for every compiler case, the correct output
                                <case>.txt, and for an accepted one the
                                correct elf.txt as <case>.elf.txt; for every
                                pipeline case, the correct output of the VM

HOW TO USE IT
-------------
  On Eustis:

    unzip parsercodegen_test.zip
    cd parsercodegen_test
    # get your parsercodegen.c, your HW2 lex.c and your HW1 vm.c into this folder
    ls parsercodegen.c lex.c vm.c      # confirms all three are here
    chmod +x test_parsercodegen.sh
    ./test_parsercodegen.sh parsercodegen.c lex.c vm.c

  1. Unzipping creates a folder named parsercodegen_test. Everything lives
     in it.
  2. Give the script all three files, in the order parsercodegen.c, lex.c,
     vm.c. Each may be a path, such as ../hw1/vm.c. Without all three
     arguments the script prints its usage line and runs nothing. The names
     are exact and case sensitive. The script refuses any other name for
     parsercodegen.c, because a submission under another name scores 0. A
     lex.c or vm.c that is missing, misnamed, or does not compile costs only
     the pipeline cases.
  3. chmod +x test_parsercodegen.sh makes the script executable. You do
     this once.
  4. Run it from inside parsercodegen_test, as often as you like. Add
     --quiet after the three file names to print only the failed cases and
     the summary.

WHAT THE SCRIPT DOES
--------------------
  It compiles parsercodegen.c, lex.c and vm.c with the course flags and
  runs every test case, each in its own empty folder:

    - most cases copy the posted <case>.tokens.txt and <case>.nametable.txt
      in as tokens.txt and nametable.txt, then run ./parsercodegen. Your
      compiler is graded on its own: a mistake in your HW2 scanner or your
      HW1 VM cannot cost these.
    - the pipeline cases (names beginning with p) copy only the source
      program in, run YOUR ./lex <case>.txt, then ./parsercodegen, then
      YOUR ./vm elf.txt (reading <case>.stdin when the case has one). These
      are the cases that need all three programs to be right.

  A compiler case earns its points only when the exit status is right AND
  the compiler's output matches expected_results/<case>.txt AND, when the
  program is accepted, the elf.txt it wrote matches
  expected_results/<case>.elf.txt. A pipeline case earns its points only
  when all three programs exit with status 0 AND the VM's output matches
  expected_results/<case>.txt. Blank lines are ignored, and so is the amount
  of spacing: any run of spaces and tabs counts as one space, and spacing at
  the start or end of a line is ignored. The text, its order, and the space
  between two values are not ignored: 7 0 203 and 7   0   203 match, but
  7 02 03 and 70203 do not.

  A compilation still running after 20 seconds is stopped. For
  parsercodegen.c the script then ends with the message "Compilation timed
  out" and no score. A lex.c or vm.c stopped this way counts as one that does
  not compile: the script warns that it does not compile, runs every other
  case, and the pipeline cases score 0. A correct file compiles on Eustis in
  well under a second.

  The whole run is limited to 15 minutes. A case still running at that
  moment is stopped and fails, and the cases after it are not run. Those
  cases score 0; the cases already finished keep their points.

  It then checks one code requirement that no case can see: an Author(s):
  line in parsercodegen.c that is no longer the placeholder. The FINAL
  SCORE it prints, out of 100, takes that into account. It is the 95
  points of the cases plus the reflection's 5, which the instructor
  deducts in Canvas when your reflection is missing or is not about this
  homework. The instructor checks the rest of the header by reading
  parsercodegen.c: every labeled line of header_comment.txt must be
  there, so compare your header with that file before you submit.

WHEN A CASE FAILS
-----------------
  student_output/<case>_output.txt          what your compiler printed (for
                                            a pipeline case, what your VM
                                            printed)
  student_output/<case>_elf.txt             the elf.txt your compiler wrote,
                                            in a pipeline case too
  student_output/<case>_lex_output.txt      what your lex printed (pipeline)
  student_output/<case>_compiler_output.txt what your compiler printed
                                            (pipeline)
  failed_cases/<case>/info.txt              what the case covers and why it
                                            failed
  failed_cases/<case>/diff.txt              your output against the expected
                                            output; lines beginning with < are
                                            yours, lines beginning with > are
                                            expected
  failed_cases/<case>/elf_diff.txt          the same for elf.txt, when your
                                            elf.txt differs

  The first line of diff.txt is where your output first departs from the
  expected output. In the Generated Code section, the instruction on that
  line or an earlier jump or call is where to look.

A PIPELINE CASE FAILS BUT ITS PROGRAM PASSES ON ITS OWN
-------------------------------------------------------
  Every pipeline program but p08 is also the program of a compiler case
  (test_cases/purposes.txt names it), so the compiler's part is checked
  there on its own. To see whether your lex writes the posted files, for
  any case:

    mkdir check && cd check
    cp ../test_cases/inputs/p02_pipeline_setx.txt .
    ../lex p02_pipeline_setx.txt > /dev/null
    diff tokens.txt ../test_cases/inputs/p02_pipeline_setx.tokens.txt
    diff nametable.txt ../test_cases/inputs/p02_pipeline_setx.nametable.txt
    cd .. && rm -r check

  (Build ../lex first with: gcc -Wall -Wextra -std=c11 -O2 lex.c -o lex)
  No output from both diff commands means your scanner wrote exactly the
  posted files. If they match, run your vm on the expected elf.txt of the
  matching compiler case and compare its output with the pipeline case's
  expected result: what differs is your VM.

  p08 is the exception: no compiler case has its program, so for p08 your
  compiler is a possible cause too. When your lex writes the posted files,
  read student_output/p08_pipeline_every_operator_elf.txt, the code your
  compiler wrote, against the code templates of the handout before you
  suspect your vm.
