#!/bin/bash

# COP 3402 HW2 - PL/0 lexical analyzer testing script
# Author: Jie Lin, Ph.D.
# Affiliation: University of Central Florida
# Course: COP 3402 - Systems Software
#
# Compiles and tests a student implementation of the PL/0 lexical analyzer.
# C only: HW2 does not accept any other language.
#
# Every test case is one line of test_cases/cases.txt. A case's standard-output
# points are earned only when BOTH of the following hold:
#
#   1. the exit status matches what the manifest requires, and
#   2. the captured output matches expected_results/<case>.txt.
#
# A case that also carries file points earns them only when tokens.txt and
# nametable.txt both exist and both match their expected contents. The two
# streams are separate: a program can earn its output points and lose its file
# points, or the reverse.
#
# Standard output totals 90 points and the two files total 10, for 100.
#
# Cases are not worth the same amount. The operators carry the most weight
# because deciding between a one-character and a two-character token is the
# decision this assignment is about, and a scanner that gets it wrong produces
# a token stream that is wrong from that point onward. The errors carry the
# next most, because there are eleven of them and each one is a different
# behavior.
#
# Standard output and standard error are captured together, so a usage message
# printed to either stream is accepted.
#
# Comparison uses "diff -a -B -w": blank lines and differences in horizontal
# whitespace are ignored, but the text and its order must match. Your columns
# do not have to line up with the reference's; the values in them do. The -a
# forces a line by line diff even when a file holds a byte diff would treat as
# binary, which happens whenever a case echoes a source program containing one.
# Without it such a case still fails correctly, but its diff.txt reads only
# "Binary files differ" and tells the student nothing.

# Configuration (update here for reuse in other projects)
SOURCE_BASENAME="lex"
DEFAULT_C_SOURCE="$SOURCE_BASENAME.c"
OUTPUT_BIN="$SOURCE_BASENAME"
EXPECTED_DIR="expected_results"
TEST_CASES_DIR="test_cases"
MANIFEST="$TEST_CASES_DIR/cases.txt"
PURPOSES="$TEST_CASES_DIR/purposes.txt"
STUDENT_OUTPUT_DIR="student_output"
FAILED_OUTPUT_DIR="failed_cases"
TOKENS_FILE="tokens.txt"
NAMES_FILE="nametable.txt"
COMPILE_TIMEOUT_SEC=20
RUN_TIMEOUT_SEC=5

# Points this script is worth on HW2. The autograder is the whole program grade.
TOTAL_POINTS=100

QUIET=0

# Color codes for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

say() {
    if [ "$QUIET" -eq 0 ]; then
        echo "$@"
    fi
}

say_status() {
    if [ "$QUIET" -eq 0 ]; then
        print_status "$1" "$2"
    fi
}

run_with_timeout() {
    local timeout=$1
    shift

    if command -v gtimeout >/dev/null 2>&1; then
        gtimeout "$timeout" "$@"
        return $?
    fi

    if command -v timeout >/dev/null 2>&1; then
        timeout "$timeout" "$@"
        return $?
    fi

    if [ -x /usr/bin/perl ]; then
        /usr/bin/perl -e 'alarm shift; exec @ARGV' "$timeout" "$@"
        return $?
    fi

    print_status $YELLOW "WARN: Timeout tool not found; running without a timeout."
    "$@"
}

is_timeout_status() {
    case $1 in
        124|137|142|143) return 0 ;;
        *) return 1 ;;
    esac
}

# The scenario a case exists to cover, from test_cases/purposes.txt. Every case
# is there to test one thing, so a failure report that says only "output
# differs" throws away the most useful fact the suite holds: what was being
# checked. Printed on every failure and written into the case's info.txt.
case_purpose() {
    [ -f "$PURPOSES" ] || return 0
    awk -F'\t' -v want="$1" '$1 == want { print $2; exit }' "$PURPOSES"
}

# Human-readable name for each category prefix used in the manifest.
category_name() {
    case "$1" in
        s) echo "Scanning basics" ;;
        p) echo "Whitespace and layout" ;;
        o) echo "Operators and longest match" ;;
        w) echo "Reserved words against identifiers" ;;
        c) echo "Comments" ;;
        n) echo "Name table" ;;
        e) echo "Lexical errors" ;;
        t) echo "Argument and file contract" ;;
        *) echo "Other" ;;
    esac
}

# Parse arguments
SOURCE_FILE=""
for arg in "$@"; do
    case "$arg" in
        --quiet) QUIET=1 ;;
        *) SOURCE_FILE="$arg" ;;
    esac
done

if [ -z "$SOURCE_FILE" ]; then
    echo "Usage: $0 <source_file> [--quiet]"
    echo "The source file must be named exactly $DEFAULT_C_SOURCE. No other name is accepted."
    echo ""
    echo "This script will:"
    echo "  1. Compile your source code with the course flags"
    echo "  2. Run every test case listed in $MANIFEST"
    echo "  3. Check the exit status, compare your output, and compare the two"
    echo "     files your scanner writes"
    echo "  4. Report your score out of $TOTAL_POINTS and show any differences"
    exit 1
fi

if [ ! -f "$SOURCE_FILE" ]; then
    print_status $RED "FAIL: Source file not found: $SOURCE_FILE"
    echo "Run this script from the directory that contains $MANIFEST,"
    echo "with your source file in that same directory."
    exit 1
fi

# The file name is part of the specification, not a convenience. It must be
# exactly lex.c: not main.c, not mylex.c, not lex.C, not LEX.c. A submission
# under any other name scores 0 however correct the code inside it is, so this
# script refuses to run rather than let a student believe otherwise.
SOURCE_BASE=$(basename -- "$SOURCE_FILE")
if [ "$SOURCE_BASE" != "$DEFAULT_C_SOURCE" ]; then
    # Webcourses appends -1, -2 and so on when a student resubmits. That rename
    # is the LMS's doing, not the student's, so take it and carry on.
    if echo "$SOURCE_BASE" | grep -qE "^${SOURCE_BASENAME}-[0-9]+\.c$"; then
        if [ -e "$DEFAULT_C_SOURCE" ] && ! cmp -s "$SOURCE_FILE" "$DEFAULT_C_SOURCE"; then
            print_status $RED "FAIL: both $SOURCE_BASE and $DEFAULT_C_SOURCE are here and they differ."
            echo "  Delete whichever one is not the submission, then run this again."
            exit 1
        fi
        print_status $YELLOW "NOTE: $SOURCE_BASE is a Webcourses resubmission name."
        echo "      Renaming it to $DEFAULT_C_SOURCE and continuing."
        cp -- "$SOURCE_FILE" "$DEFAULT_C_SOURCE"
        SOURCE_FILE="$DEFAULT_C_SOURCE"
        SOURCE_BASE="$DEFAULT_C_SOURCE"
        say ""
    else
        print_status $RED "FAIL: Wrong file name: $SOURCE_BASE"
        echo ""
        echo "  HW2 requires a file named exactly '$DEFAULT_C_SOURCE'."
        echo "  You gave:    $SOURCE_BASE"
        echo "  Rename it:   mv $SOURCE_BASE $DEFAULT_C_SOURCE"
        echo ""
        echo "  The name is graded. A submission under any other name scores 0,"
        echo "  even when the program itself is completely correct. The comparison"
        echo "  is exact and case sensitive: lex.c is accepted, LEX.c and lex.C are not."
        exit 1
    fi
fi

say_status $BLUE "INFO: Compiling C source file: $SOURCE_FILE"
rm -f -- "$OUTPUT_BIN"
run_with_timeout "$COMPILE_TIMEOUT_SEC" gcc -Wall -Wextra -std=c11 -O2 "$SOURCE_FILE" -o "$OUTPUT_BIN"
compile_status=$?

if is_timeout_status $compile_status; then
    print_status $RED "FAIL: Compilation timed out after ${COMPILE_TIMEOUT_SEC}s"
    exit 1
fi
if [ $compile_status -ne 0 ]; then
    print_status $RED "FAIL: Compilation of $SOURCE_FILE failed"
    echo "Please fix the compilation errors and try again."
    echo "Nothing else in this script can run until your program compiles."
    exit 1
fi

EXE="./$OUTPUT_BIN"
print_status $GREEN "PASS: Compilation of $SOURCE_FILE succeeded"
say ""

if [ ! -d "$EXPECTED_DIR" ]; then
    print_status $RED "FAIL: Expected results directory '$EXPECTED_DIR' not found!"
    echo "Run this script from inside the unzipped ${SOURCE_BASENAME}_test directory."
    rm -f -- "$OUTPUT_BIN"
    exit 1
fi

if [ ! -f "$MANIFEST" ]; then
    print_status $RED "FAIL: Test case manifest '$MANIFEST' not found!"
    rm -f -- "$OUTPUT_BIN"
    exit 1
fi

# Create output directories (fresh each run)
rm -rf "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"
mkdir -p "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
EARNED_OUT=0
EARNED_FILES=0
POSSIBLE_OUT=0
POSSIBLE_FILES=0
CATEGORY_KEYS=""
FAILED_NAMES=""

say_status $CYAN "INFO: Starting test evaluation..."
say "============================================================"

record_failure() {
    local test_name=$1
    local reason=$2
    local command_line=$3
    local expected_file=$4
    local student_output=$5

    local case_dir="$FAILED_OUTPUT_DIR/$test_name"
    mkdir -p "$case_dir"
    {
        echo "Test Case: $test_name"
        info_purpose=$(case_purpose "$test_name")
        [ -n "$info_purpose" ] && echo "What this case covers: $info_purpose"
        echo "Reason: $reason"
        echo "Command: $command_line"
        echo "Expected: $expected_file"
        echo "Your Output: $student_output"
    } > "$case_dir/info.txt"

    # Write the diff for every recorded failure, not only output mismatches. A
    # case that failed on exit status alone almost always has wrong output too,
    # and the diff is the fastest way for a student to see what changed.
    if [ -f "$student_output" ] && [ -f "$expected_file" ]; then
        diff -a -B -w "$student_output" "$expected_file" > "$case_dir/diff.txt" 2>/dev/null
    fi
}

add_points() {
    # add_points <variable-name-suffix> <amount>
    local key=$1
    local amount=$2
    local current
    eval "current=\${CAT_$key:-0}"
    eval "CAT_$key=\$(awk -v a=\"\$current\" -v b=\"\$amount\" 'BEGIN { printf \"%g\", a + b }')"
    case " $CATEGORY_KEYS " in
        *" $key "*) ;;
        *) CATEGORY_KEYS="$CATEGORY_KEYS $key" ;;
    esac
}

# Compares one file the scanner wrote against its expected contents. Prints its
# own diagnosis and returns non-zero on any mismatch.
compare_written_file() {
    local test_name=$1
    local produced=$2
    local expected=$3
    local case_dir="$FAILED_OUTPUT_DIR/$test_name"

    if [ ! -f "$expected" ]; then
        print_status $RED "   FAIL: expected file $expected not found"
        return 1
    fi
    if [ ! -f "$produced" ]; then
        print_status $RED "   FAIL: your program did not write $produced"
        echo "        The name is part of the specification and is case sensitive."
        return 1
    fi
    if diff -a -B -w "$produced" "$expected" > /dev/null 2>&1; then
        return 0
    fi

    print_status $RED "   FAIL: $produced differs from expected"
    mkdir -p "$case_dir"
    cp -- "$produced" "$case_dir/$(basename -- "$produced")" 2>/dev/null
    diff -a -B -w "$produced" "$expected" > "$case_dir/$(basename -- "$produced").diff" 2>/dev/null
    diff -a -B -w "$produced" "$expected" | head -6
    return 1
}

run_test_case() {
    local test_name=$1
    local expected_exit=$2
    local out_points=$3
    local file_points=$4
    shift 4
    local args=("$@")

    local expected_file="$EXPECTED_DIR/$test_name.txt"
    local expected_tokens="$EXPECTED_DIR/$test_name.tokens.txt"
    local expected_names="$EXPECTED_DIR/$test_name.nametable.txt"
    local student_output="$STUDENT_OUTPUT_DIR/${test_name}_output.txt"
    local command_line="$EXE ${args[*]}"
    local prefix="${test_name:0:1}"
    local case_ok=1

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    POSSIBLE_OUT=$(awk -v a="$POSSIBLE_OUT" -v b="$out_points" 'BEGIN { printf "%g", a + b }')
    POSSIBLE_FILES=$(awk -v a="$POSSIBLE_FILES" -v b="$file_points" 'BEGIN { printf "%g", a + b }')
    add_points "${prefix}_possible" "$out_points"
    add_points "${prefix}_possible" "$file_points"

    if [ "$file_points" != "0" ]; then
        say_status $PURPLE "INFO: Test Case: $test_name   ($out_points output + $file_points file points)"
    else
        say_status $PURPLE "INFO: Test Case: $test_name   ($out_points points)"
    fi
    say "   Command: $command_line"
    if [ "$expected_exit" = "0" ]; then
        say "   Required exit status: 0"
    else
        say "   Required exit status: any value other than 0"
    fi

    if [ ! -f "$expected_file" ]; then
        print_status $RED "   FAIL: Expected result file $expected_file not found"
        record_failure "$test_name" "Missing expected result file" "$command_line" "$expected_file" ""
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_NAMES="$FAILED_NAMES $test_name"
        return 1
    fi

    # The scanner writes into the working directory. Clear the previous case's
    # files so this case is judged on what it wrote itself.
    rm -f -- "./$TOKENS_FILE" "./$NAMES_FILE"

    run_with_timeout "$RUN_TIMEOUT_SEC" "$EXE" ${args[@]+"${args[@]}"} < /dev/null > "$student_output" 2>&1
    local run_status=$?

    if is_timeout_status $run_status; then
        print_status $RED "   FAIL: Program timed out after ${RUN_TIMEOUT_SEC}s"
        purpose=$(case_purpose "$test_name")
        [ -n "$purpose" ] && echo "   This case covers: $purpose"
        echo "   Partial output saved to: $student_output"
        record_failure "$test_name" "Program timed out" "$command_line" "$expected_file" "$student_output"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_NAMES="$FAILED_NAMES $test_name"
        return 1
    fi

    # Exit status and standard output together decide the output points.
    if [ "$expected_exit" = "0" ] && [ $run_status -ne 0 ]; then
        print_status $RED "   FAIL: Expected exit status 0, your program exited with $run_status"
        purpose=$(case_purpose "$test_name")
        [ -n "$purpose" ] && echo "   This case covers: $purpose"
        echo "   Your output: $student_output"
        record_failure "$test_name" "Wrong exit status (expected 0, got $run_status)" \
                       "$command_line" "$expected_file" "$student_output"
        case_ok=0
    elif [ "$expected_exit" = "nonzero" ] && [ $run_status -eq 0 ]; then
        print_status $RED "   FAIL: Expected a nonzero exit status, your program exited with 0"
        purpose=$(case_purpose "$test_name")
        [ -n "$purpose" ] && echo "   This case covers: $purpose"
        echo "   This case contains a lexical error on purpose. Your scanner must report"
        echo "   it and leave with a nonzero status instead of finishing normally."
        echo "   Your output: $student_output"
        record_failure "$test_name" "Wrong exit status (expected nonzero, got 0)" \
                       "$command_line" "$expected_file" "$student_output"
        case_ok=0
    elif diff -a -B -w "$student_output" "$expected_file" > /dev/null 2>&1; then
        EARNED_OUT=$(awk -v a="$EARNED_OUT" -v b="$out_points" 'BEGIN { printf "%g", a + b }')
        add_points "${prefix}_earned" "$out_points"
        say_status $GREEN "   PASS: Exit status correct and output matches (exit $run_status)"
    else
        print_status $RED "   FAIL: Output differs from expected result"
        purpose=$(case_purpose "$test_name")
        [ -n "$purpose" ] && echo "   This case covers: $purpose"
        echo "   Your output:      $student_output"
        echo "   Expected output:  $expected_file"
        echo "   First few differences (blank lines and whitespace ignored):"
        diff -a -B -w "$student_output" "$expected_file" | head -10
        record_failure "$test_name" "Output mismatch" "$command_line" "$expected_file" "$student_output"
        case_ok=0
    fi

    # File points are a separate stream. A case can lose its output points and
    # still earn these, and the reverse.
    if [ "$file_points" != "0" ]; then
        local files_ok=1
        compare_written_file "$test_name" "./$TOKENS_FILE" "$expected_tokens" || files_ok=0
        compare_written_file "$test_name" "./$NAMES_FILE" "$expected_names"  || files_ok=0
        if [ "$files_ok" -eq 1 ]; then
            EARNED_FILES=$(awk -v a="$EARNED_FILES" -v b="$file_points" 'BEGIN { printf "%g", a + b }')
            add_points "${prefix}_earned" "$file_points"
            say_status $GREEN "   PASS: $TOKENS_FILE and $NAMES_FILE both match"
        else
            case_ok=0
        fi
    fi

    if [ "$case_ok" -eq 1 ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    fi
    FAILED_TESTS=$((FAILED_TESTS + 1))
    FAILED_NAMES="$FAILED_NAMES $test_name"
    return 1
}

say "INFO: Reading test cases from $MANIFEST..."
say ""

while IFS=$'\t' read -r -a fields || [ ${#fields[@]} -gt 0 ]; do
    if [ ${#fields[@]} -eq 0 ]; then
        continue
    fi
    case "${fields[0]}" in
        ''|'#'*) continue ;;
    esac
    if [ ${#fields[@]} -lt 4 ]; then
        print_status $YELLOW "   WARN: Skipping malformed manifest line: ${fields[0]}"
        continue
    fi

    run_test_case "${fields[0]}" "${fields[1]}" "${fields[2]}" "${fields[3]}" "${fields[@]:4}"
    say ""
done < "$MANIFEST"

rm -f -- "./$TOKENS_FILE" "./$NAMES_FILE"

SCORE=$(awk -v a="$EARNED_OUT" -v b="$EARNED_FILES" 'BEGIN { printf "%.2f", a + b }')
POSSIBLE=$(awk -v a="$POSSIBLE_OUT" -v b="$POSSIBLE_FILES" 'BEGIN { printf "%g", a + b }')
COVERAGE=$(awk -v e="$SCORE" -v p="$POSSIBLE" 'BEGIN { if (p == 0) print "0.0"; else printf "%.1f", (e / p) * 100 }')

SUMMARY_COLOR=$GREEN
if [ $FAILED_TESTS -ne 0 ]; then
    SUMMARY_COLOR=$RED
fi

echo "============================================================"
print_status $SUMMARY_COLOR "Test Summary"
echo "============================================================"
echo "   Total Tests:       $TOTAL_TESTS"
echo "   Passed:            $PASSED_TESTS"
echo "   Failed:            $FAILED_TESTS"
echo ""
printf '   Standard output:   %s / %s\n' "$EARNED_OUT" "$POSSIBLE_OUT"
printf '   Written files:     %s / %s\n' "$EARNED_FILES" "$POSSIBLE_FILES"
print_status $SUMMARY_COLOR "   HW2 program score: ${SCORE} / ${TOTAL_POINTS}"
echo ""
echo "   Specified scanner behavior your program implements correctly: ${COVERAGE}%"
echo "   (This suite covers 100% of the scanner this handout specifies."
echo "    The figure above is how much of it your program gets right.)"
echo ""
echo "   By category:"
for key in $CATEGORY_KEYS; do
    case "$key" in
        *_possible)
            prefix="${key%%_*}"
            eval "poss=\${CAT_${prefix}_possible:-0}"
            eval "earn=\${CAT_${prefix}_earned:-0}"
            printf '     %-38s %5s / %-5s\n' "$(category_name "$prefix")" "$earn" "$poss"
            ;;
    esac
done

# ----------------------------------------------------------------------------
# The rules a test cannot see. The handout lists each of these under Automatic
# Zero, so they belong in the same report as the score, not in a separate pass.
# Argument handling is not here: it is scored by the t cases above, and a
# condition that is an automatic zero is never also a scored case.
# ----------------------------------------------------------------------------
VIOLATIONS=""
grep -qF '<Your Name Here>' "$SOURCE_FILE" && VIOLATIONS="$VIOLATIONS|the Author(s): line still reads <Your Name Here>"

echo ""
echo "   Code requirements:"
if [ -z "$VIOLATIONS" ]; then
    print_status $GREEN "     all clear (file name correct, compiles with the course flags, author set)"
    FINAL_SCORE="$SCORE"
else
    echo "$VIOLATIONS" | tr '|' '\n' | while read -r v; do
        [ -n "$v" ] && print_status $RED "     $v"
    done
    FINAL_SCORE="0.00"
fi

echo ""
echo "============================================================"
if [ -z "$VIOLATIONS" ]; then
    print_status $SUMMARY_COLOR "   FINAL SCORE: ${FINAL_SCORE} / ${TOTAL_POINTS}"
else
    print_status $RED "   FINAL SCORE: 0.00 / ${TOTAL_POINTS}"
    echo "   The test score above does not apply: each item under Code"
    echo "   requirements is an automatic zero in the handout."
fi
echo "============================================================"

if [ $FAILED_TESTS -ne 0 ]; then
    echo ""
    echo "   Cases that failed:"
    for name in $FAILED_NAMES; do
        echo "     $name"
    done
    echo ""
    echo "   Details for every failed case are in:"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/info.txt"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/diff.txt"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/$TOKENS_FILE.diff     (when file points were lost)"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/$NAMES_FILE.diff      (when file points were lost)"
    echo ""
    echo "   Lines beginning with \"<\" are yours. Lines beginning with \">\" are expected."
    rm -f -- "$OUTPUT_BIN"
    exit 1
fi

rm -f -- "$OUTPUT_BIN"
exit 0
