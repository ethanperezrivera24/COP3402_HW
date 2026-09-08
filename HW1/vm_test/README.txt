COP 3402 - Systems Software
HW1 test bundle for the PM/0 virtual machine

WHAT IS IN HERE
---------------
  test_vm.sh                    the autograder script
  header_comment.txt            the header block required by the handout
  test_cases/cases.txt          the list of test cases the script runs,
                                with what each one is worth
  test_cases/inputs/            the PM/0 programs the test cases run,
                                plus a .stdin file for the cases that read
  expected_results/             the correct output for every test case

HOW TO USE IT
-------------
  On Eustis:

    unzip vm_test.zip
    cd vm_test
    # get your vm.c into this folder, however you like
    ls vm.c            # confirms it is here
    chmod +x test_vm.sh
    ./test_vm.sh vm.c

  1. Unzipping creates a folder named vm_test. Everything lives in it.
  2. Your vm.c has to end up DIRECTLY INSIDE that folder, next to
     test_vm.sh. How it gets there is up to you: cp it, scp it, drag and
     drop it, or write it there in an editor. Nothing depends on the
     method. What matters is that vm.c is in the folder, and "ls vm.c"
     is how you confirm it.

     The file must be named vm.c. Exactly that, in lower case. The
     check is case sensitive, so VM.c and vm.C are rejected along with
     main.c, myvm.c and every other name. A submission under any other
     name scores 0 even when the program is correct, so the script
     refuses to run rather than let you find out after the deadline.
  3. chmod +x test_vm.sh makes the script executable. You only do this
     once per unzipped copy.
  4. Run it from inside vm_test, as often as you like. The script looks
     for test_cases/ and expected_results/ in the directory you run it
     from, so leave test_vm.sh where it is.

WHAT THE SCRIPT DOES
--------------------
  It compiles your program with the course flags, runs all twenty-eight
  test cases, checks the exit status of each run, compares your output
  against expected_results/, and reports your score out of 100.

  It then checks the code requirements no test can see: at most three
  functions, no heap allocation, no function-like macros, and an Author:
  line that is no longer the placeholder.

  No heap allocation means no call to malloc, calloc, realloc or free.
  It does not mean no pointers: fopen returns a FILE * and you need it
  to read the instruction file. Close it with fclose. It did not come
  from malloc, so do not pass it to free. Each of those is an
  automatic zero in the handout, so the script prints a FINAL SCORE that
  takes them into account. That final score is what the grader records.

  If Webcourses renamed your file to vm-1.c on a resubmission, the script
  renames it back to vm.c and carries on. That is the only name besides
  vm.c it will accept.

  The cases are not worth the same amount. Control flow and procedure
  linkage carry 35 points, memory and stack discipline 20, error handling
  20, the arithmetic and relational operators 15, and the trace format and
  I/O contract 10. Every point value is in test_cases/cases.txt, in the
  open, before you write a line.

  Nine cases run the machine into an error on purpose: division by zero,
  an unknown opcode, an unknown OPR sub-operation, an unknown SYS
  operation, a stack that grows into the code, a STO and a LOD whose
  address falls outside the stack, a program too large for the text
  segment, and a jump that leaves the text segment. Each must print its
  message and exit with a non-zero status. Three more run the program with
  the wrong number of arguments or a file that does not exist.

WHERE TO LOOK WHEN A CASE FAILS
-------------------------------
  student_output/<case>_output.txt    exactly what your program printed
  failed_cases/<case>/info.txt        the command that was run
  failed_cases/<case>/diff.txt        your output on the left, expected on the right

  Both directories are deleted and rebuilt every time the script runs.

  Lines beginning with "<" are yours. Lines beginning with ">" are expected.

NOTES
-----
  Standard output and standard error are captured together, so it does not
  matter which stream you print your error and usage messages to.

  Comparison ignores blank lines and differences in spaces and tabs. Your
  columns do not have to line up with the reference's; the values in them
  do, and so does their order.

  You may run this script as many times as you want. Nothing is recorded
  and nothing is submitted from here. Do not modify the files in
  test_cases/ or expected_results/; the instructor grades against the
  original copies, and a case you edited into passing still fails there.
