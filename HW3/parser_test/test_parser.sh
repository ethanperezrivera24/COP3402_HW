#!/bin/bash

# COP 3402 HW3 - PL/0 parser and symbol table testing script
# Author: Jie Lin, Ph.D.
# Affiliation: University of Central Florida
# Course: COP 3402 - Systems Software
#
# Compiles and tests a student's parser.c, and their HW2 lex.c for the
# pipeline cases. C only.
#
# Usage, from inside the unzipped parser_test folder:
#   ./test_parser.sh parser.c lex.c [--quiet]
# Both files are required arguments, the parser first and the HW2 scanner
# second; either may be a path. A lex.c that is missing, misnamed, or does
# not compile costs only the pipeline cases.
#
# Every test case is one line of test_cases/cases.txt and runs in its own
# empty working directory, holding only what the case's mode provides:
#
#   files        tokens.txt and nametable.txt, copied from the case's posted
#                <case>.tokens.txt and <case>.nametable.txt; then ./parser
#   names-only   only nametable.txt; then ./parser
#   tokens-only  only tokens.txt; then ./parser
#   pipeline     only the source program <case>.txt; then your ./lex <case>.txt
#                writes the two files, and then ./parser reads them
#
# A case earns its points only when BOTH of the following hold:
#
#   1. the exit status is what the manifest requires: 0, or for an error
#      case a status from 1 to 123 (a crash or a timeout is not an error
#      report), and
#   2. the parser's standard output matches expected_results/<case>.txt.
#
# Comparison uses "diff -a -B -w": blank lines and differences in horizontal
# whitespace are ignored, but the text and its order must match. Your columns
# do not have to line up with the reference's; the values in them do.
#
# Each run of a program gets an empty standard input, a time limit, a forced
# kill a few seconds after the limit for a program that ignores the first
# signal, and a cap on how much it may write.
#
# The whole run of all the cases is limited to 15 minutes, so a parser that
# loops or crawls cannot hold a grader for longer. A case still running when
# the limit is reached is stopped and fails, and every case after it is not
# run. Those cases score 0; the cases already finished keep their points.

# Configuration
SOURCE_BASENAME="parser"
DEFAULT_C_SOURCE="$SOURCE_BASENAME.c"
LEX_BASENAME="lex"
DEFAULT_LEX_SOURCE="$LEX_BASENAME.c"
EXPECTED_DIR="expected_results"
TEST_CASES_DIR="test_cases"
INPUTS_DIR="$TEST_CASES_DIR/inputs"
MANIFEST="$TEST_CASES_DIR/cases.txt"
PURPOSES="$TEST_CASES_DIR/purposes.txt"
STUDENT_OUTPUT_DIR="student_output"
FAILED_OUTPUT_DIR="failed_cases"
COMPILE_TIMEOUT_SEC=20
RUN_TIMEOUT_SEC=10      # the time limit of one program run
KILL_GRACE_SEC=5        # SIGKILL this long after the SIGTERM at the limit
OUTPUT_LIMIT_KB=10240   # the cap on what one program run may write; the
                        # largest expected result is about 110 KB
TOTAL_TIME_LIMIT_SEC=900  # the limit on running all the cases together
TOTAL_LIMIT_MIN=$(( TOTAL_TIME_LIMIT_SEC / 60 ))
CFLAGS="-Wall -Wextra -std=c11 -O2"

TOTAL_POINTS=100
QUIET=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

print_status() { echo -e "${1}${2}${NC}"; }
say() { [ "$QUIET" -eq 0 ] && echo "$@"; return 0; }
say_status() { [ "$QUIET" -eq 0 ] && print_status "$1" "$2"; return 0; }

# timeout -k <grace> <limit> where available; gtimeout on macOS; a perl alarm
# after that.
run_with_timeout() {
    local limit=$1
    shift
    if command -v timeout >/dev/null 2>&1; then
        timeout -k "$KILL_GRACE_SEC" "$limit" "$@"
        return $?
    fi
    if command -v gtimeout >/dev/null 2>&1; then
        gtimeout -k "$KILL_GRACE_SEC" "$limit" "$@"
        return $?
    fi
    if command -v perl >/dev/null 2>&1; then
        perl -e 'alarm shift; exec @ARGV' "$limit" "$@"
        return $?
    fi
    print_status "$YELLOW" "WARN: no timeout tool found; running without a time limit."
    "$@"
}

is_timeout_status() {
    case $1 in 124|137|142|143) return 0 ;; *) return 1 ;; esac
}

case_purpose() {
    [ -f "$PURPOSES" ] || return 0
    awk -F'\t' -v want="$1" '$1 == want { print $2; exit }' "$PURPOSES"
}

category_name() {
    case "$1" in
        a) echo "Accepted programs" ;;
        g) echo "Syntax errors" ;;
        y) echo "Duplicate, undeclared, and kind errors" ;;
        l) echo "Entry limit and missing files" ;;
        p) echo "Pipeline: your lex, then your parser" ;;
        *) echo "Other" ;;
    esac
}

# Accept the required name, or the Webcourses resubmission name
# <base>-<digits>.c, which is renamed back. Prints the name to use.
resolve_name() {
    local given=$1 base=$2 required=$3
    local name
    name=$(basename -- "$given")
    if [ "$name" = "$required" ]; then
        echo "$given"
        return 0
    fi
    if echo "$name" | grep -qE "^${base}-[0-9]+\.c$"; then
        if [ -e "$required" ] && ! cmp -s "$given" "$required"; then
            print_status "$RED" "FAIL: both $name and $required are here and they differ." >&2
            echo "  Delete whichever one is not the submission, then run this again." >&2
            return 1
        fi
        print_status "$YELLOW" "NOTE: $name is a Webcourses resubmission name; renaming it to $required." >&2
        cp -- "$given" "$required"
        echo "$required"
        return 0
    fi
    print_status "$RED" "FAIL: Wrong file name: $name" >&2
    echo "  This homework requires a file named exactly '$required'." >&2
    echo "  Rename it:   mv $name $required" >&2
    echo "  The name is graded: a submission under any other name scores 0," >&2
    echo "  even when the program is correct. lex.c is accepted; Lex.c is not." >&2
    return 1
}

# ------------------------------------------------------------------ arguments
FILES=()
for arg in "$@"; do
    case "$arg" in
        --quiet) QUIET=1 ;;
        *) FILES+=("$arg") ;;
    esac
done

if [ ${#FILES[@]} -ne 2 ]; then
    echo "Usage: $0 parser.c lex.c [--quiet]"
    echo "Give both files: your parser first, then your HW2 scanner. Either may be a path."
    echo "They must be named exactly $DEFAULT_C_SOURCE and $DEFAULT_LEX_SOURCE."
    echo ""
    echo "This script will:"
    echo "  1. Compile parser.c and lex.c with the course flags"
    echo "  2. Run every test case listed in $MANIFEST"
    echo "  3. Check each exit status and compare each output"
    echo "  4. Report your score out of $TOTAL_POINTS and show any differences"
    exit 1
fi
SOURCE_FILE=${FILES[0]}
LEX_FILE=${FILES[1]}
if [ ! -f "$SOURCE_FILE" ]; then
    print_status "$RED" "FAIL: Source file not found: $SOURCE_FILE"
    echo "Run this script from the directory that contains $MANIFEST,"
    echo "with your source files in that same directory."
    exit 1
fi
SOURCE_FILE=$(resolve_name "$SOURCE_FILE" "$SOURCE_BASENAME" "$DEFAULT_C_SOURCE") || exit 1

if [ ! -d "$EXPECTED_DIR" ] || [ ! -f "$MANIFEST" ]; then
    print_status "$RED" "FAIL: $EXPECTED_DIR/ or $MANIFEST not found."
    echo "Run this script from inside the unzipped ${SOURCE_BASENAME}_test folder."
    exit 1
fi

# ------------------------------------------------------------------- compile
say_status "$BLUE" "INFO: Compiling $SOURCE_FILE"
rm -f -- "$SOURCE_BASENAME"
run_with_timeout "$COMPILE_TIMEOUT_SEC" gcc $CFLAGS "$SOURCE_FILE" -o "$SOURCE_BASENAME"
compile_status=$?
if is_timeout_status $compile_status; then
    print_status "$RED" "FAIL: Compilation timed out after ${COMPILE_TIMEOUT_SEC}s"
    exit 1
fi
if [ $compile_status -ne 0 ] || [ ! -x "$SOURCE_BASENAME" ]; then
    print_status "$RED" "FAIL: Compilation of $SOURCE_FILE failed"
    echo "A parser.c that does not compile on Eustis scores 0. Fix the errors and run this again."
    exit 1
fi
print_status "$GREEN" "PASS: Compilation of $SOURCE_FILE succeeded"
EXE="$PWD/$SOURCE_BASENAME"

# lex.c is needed only by the pipeline cases. Without it, those cases score 0
# and every other case still runs.
LEX_EXE=""
LEX_PROBLEM=""
if [ ! -f "$LEX_FILE" ]; then
    LEX_PROBLEM="$LEX_FILE not found"
elif ! LEX_FILE=$(resolve_name "$LEX_FILE" "$LEX_BASENAME" "$DEFAULT_LEX_SOURCE"); then
    LEX_PROBLEM="lex.c has the wrong file name"
else
    say_status "$BLUE" "INFO: Compiling $LEX_FILE (your HW2 scanner, for the pipeline cases)"
    rm -f -- "$LEX_BASENAME"
    run_with_timeout "$COMPILE_TIMEOUT_SEC" gcc $CFLAGS "$LEX_FILE" -o "$LEX_BASENAME"
    if [ $? -eq 0 ] && [ -x "$LEX_BASENAME" ]; then
        LEX_EXE="$PWD/$LEX_BASENAME"
        print_status "$GREEN" "PASS: Compilation of $LEX_FILE succeeded"
    else
        LEX_PROBLEM="$LEX_FILE does not compile"
    fi
fi
if [ -n "$LEX_PROBLEM" ]; then
    print_status "$YELLOW" "WARN: $LEX_PROBLEM; the pipeline (p) cases will score 0."
fi
say ""

rm -rf "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"
mkdir -p "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"
SCRATCH=$(mktemp -d 2>/dev/null || mktemp -d -t parser_test)
trap 'rm -rf "$SCRATCH"' EXIT

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
EARNED=0
POSSIBLE=0
CATEGORY_KEYS=""
FAILED_NAMES=""
NOT_RUN=0

add_points() {
    local key=$1 amount=$2 current
    eval "current=\${CAT_$key:-0}"
    eval "CAT_$key=\$(awk -v a=\"\$current\" -v b=\"\$amount\" 'BEGIN { printf \"%g\", a + b }')"
    case " $CATEGORY_KEYS " in *" $key "*) ;; *) CATEGORY_KEYS="$CATEGORY_KEYS $key" ;; esac
}

record_failure() {
    local name=$1 reason=$2 expected=$3 output=$4
    local dir="$FAILED_OUTPUT_DIR/$name"
    mkdir -p "$dir"
    {
        echo "Test Case: $name"
        local purpose
        purpose=$(case_purpose "$name")
        [ -n "$purpose" ] && echo "What this case covers: $purpose"
        echo "Reason: $reason"
        echo "Expected: $expected"
        echo "Your Output: $output"
    } > "$dir/info.txt"
    if [ -f "$output" ] && [ -f "$expected" ]; then
        diff -a -B -w "$output" "$expected" > "$dir/diff.txt" 2>/dev/null
    fi
}

# Runs one program in a case's working directory with an empty standard
# input, the time limit, the forced kill, and the output cap. The subshell
# keeps the cap away from this script.
run_capped() {
    local dir=$1 out=$2 err=$3 limit=$4
    shift 4
    ( ulimit -S -f "$OUTPUT_LIMIT_KB" 2>/dev/null
      cd "$dir" && run_with_timeout "$limit" "$@" ) < /dev/null > "$out" 2> "$err"
}

# Seconds left of the whole run's limit, and the time limit of the next
# program run: the usual limit, or what is left of the whole run if less.
# The limit is never below 1, because a limit of 0 turns timeout off.
time_left() { echo $(( TOTAL_TIME_LIMIT_SEC - (SECONDS - RUN_START) )); }
next_limit() {
    local left
    left=$(time_left)
    if [ "$left" -lt 1 ]; then echo 1
    elif [ "$left" -lt "$RUN_TIMEOUT_SEC" ]; then echo "$left"
    else echo "$RUN_TIMEOUT_SEC"; fi
}

explain_status() {    # explain_status <status> <time limit the run had>
    case $1 in
        124|137|142|143)
            if [ "${2:-$RUN_TIMEOUT_SEC}" -lt "$RUN_TIMEOUT_SEC" ]; then
                echo "it was stopped when the whole run reached its ${TOTAL_LIMIT_MIN}-minute limit"
            else
                echo "it ran past the ${RUN_TIMEOUT_SEC}-second limit"
            fi ;;
        153) echo "it wrote more than the ${OUTPUT_LIMIT_KB} KB output limit" ;;
        *) if [ "$1" -ge 128 ]; then echo "it was killed by signal $(( $1 - 128 )) (a crash is not an error report)"
           else echo "it exited with status $1"; fi ;;
    esac
}

run_test_case() {
    local name=$1 want=$2 points=$3 mode=$4
    local expected="$EXPECTED_DIR/$name.txt"
    local output="$STUDENT_OUTPUT_DIR/${name}_output.txt"
    local prefix="${name:0:1}"
    local dir="$SCRATCH/$name"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    POSSIBLE=$(awk -v a="$POSSIBLE" -v b="$points" 'BEGIN { printf "%g", a + b }')
    add_points "${prefix}_possible" "$points"

    say_status "$PURPLE" "INFO: Test Case: $name   ($points points, mode $mode)"
    if [ "$want" = "0" ]; then say "   Required exit status: 0"
    else say "   Required exit status: 1"; fi

    fail_case() {
        print_status "$RED" "   FAIL: $1"
        local purpose
        purpose=$(case_purpose "$name")
        [ -n "$purpose" ] && echo "   This case covers: $purpose"
        record_failure "$name" "$1" "$expected" "$output"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_NAMES="$FAILED_NAMES $name"
    }

    if [ ! -f "$expected" ]; then
        fail_case "expected result $expected not found"
        return
    fi
    if [ "$(time_left)" -le 0 ]; then
        NOT_RUN=$((NOT_RUN + 1))
        fail_case "not run: the whole run reached its ${TOTAL_LIMIT_MIN}-minute limit"
        return
    fi

    local limit
    rm -rf "$dir" && mkdir -p "$dir"
    case "$mode" in
        files)
            cp "$INPUTS_DIR/$name.tokens.txt" "$dir/tokens.txt"
            cp "$INPUTS_DIR/$name.nametable.txt" "$dir/nametable.txt" ;;
        names-only)
            cp "$INPUTS_DIR/$name.nametable.txt" "$dir/nametable.txt" ;;
        tokens-only)
            cp "$INPUTS_DIR/$name.tokens.txt" "$dir/tokens.txt" ;;
        pipeline)
            : > "$output"
            if [ -z "$LEX_EXE" ]; then
                fail_case "pipeline case not run: $LEX_PROBLEM"
                return
            fi
            cp "$INPUTS_DIR/$name.txt" "$dir/"
            say "   Command: ./lex $name.txt && ./parser"
            limit=$(next_limit)
            run_capped "$dir" "$STUDENT_OUTPUT_DIR/${name}_lex_output.txt" /dev/null "$limit" "$LEX_EXE" "$name.txt"
            local lex_status=$?
            if [ $lex_status -ne 0 ]; then
                fail_case "your lex failed on $name.txt: $(explain_status $lex_status "$limit"); its output is in $STUDENT_OUTPUT_DIR/${name}_lex_output.txt"
                return
            fi
            if [ "$(time_left)" -le 0 ]; then
                NOT_RUN=$((NOT_RUN + 1))
                fail_case "parser not run: the whole run reached its ${TOTAL_LIMIT_MIN}-minute limit"
                return
            fi ;;
        *)
            fail_case "unknown mode $mode in the manifest"
            return ;;
    esac
    [ "$mode" = "pipeline" ] || say "   Command: ./parser"

    limit=$(next_limit)
    run_capped "$dir" "$output" "$dir/.stderr" "$limit" "$EXE"
    local status=$?

    if [ "$want" = "0" ] && [ $status -ne 0 ]; then
        fail_case "exit status should be 0, but $(explain_status $status "$limit")"
    elif [ "$want" != "0" ] && { [ $status -lt 1 ] || [ $status -gt 123 ]; }; then
        if [ $status -eq 0 ]; then
            fail_case "exit status should be 1 (this case holds an error on purpose), but it exited with 0"
        else
            fail_case "exit status should be 1, but $(explain_status $status "$limit")"
        fi
    elif diff -a -B -w "$output" "$expected" > /dev/null 2>&1; then
        EARNED=$(awk -v a="$EARNED" -v b="$points" 'BEGIN { printf "%g", a + b }')
        add_points "${prefix}_earned" "$points"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        say_status "$GREEN" "   PASS: exit status correct and output matches (exit $status)"
    else
        fail_case "output differs from $expected"
        echo "   First differences (blank lines and whitespace ignored):"
        diff -a -B -w "$output" "$expected" | head -10
    fi
}

say_status "$CYAN" "INFO: Reading test cases from $MANIFEST"
say "INFO: The whole run is limited to $TOTAL_LIMIT_MIN minutes; cases not finished by then score 0."
RUN_START=$SECONDS
say "============================================================"
while IFS=$'\t' read -r -a fields || [ ${#fields[@]} -gt 0 ]; do
    [ ${#fields[@]} -eq 0 ] && continue
    case "${fields[0]}" in ''|'#'*) continue ;; esac
    if [ ${#fields[@]} -lt 4 ]; then
        print_status "$YELLOW" "   WARN: skipping malformed manifest line: ${fields[0]}"
        continue
    fi
    run_test_case "${fields[0]}" "${fields[1]}" "${fields[2]}" "${fields[3]}"
    say ""
done < "$MANIFEST"

SCORE=$(awk -v a="$EARNED" 'BEGIN { printf "%.2f", a }')
SUMMARY_COLOR=$GREEN
[ $FAILED_TESTS -ne 0 ] && SUMMARY_COLOR=$RED

echo "============================================================"
print_status "$SUMMARY_COLOR" "Test Summary"
echo "============================================================"
echo "   Total Tests:       $TOTAL_TESTS"
echo "   Passed:            $PASSED_TESTS"
echo "   Failed:            $FAILED_TESTS"
if [ $NOT_RUN -gt 0 ]; then
    print_status "$RED" "   STOPPED: the whole run reached its $TOTAL_LIMIT_MIN-minute limit; $NOT_RUN cases were not run and score 0."
fi
echo ""
echo "   By category:"
for key in $CATEGORY_KEYS; do
    case "$key" in
        *_possible)
            prefix="${key%%_*}"
            eval "poss=\${CAT_${prefix}_possible:-0}"
            eval "earn=\${CAT_${prefix}_earned:-0}"
            printf '     %-40s %6s / %-6s\n' "$(category_name "$prefix")" "$earn" "$poss"
            ;;
    esac
done
print_status "$SUMMARY_COLOR" "   HW3 test score: ${SCORE} / ${TOTAL_POINTS}"

# The rule a test cannot see: the handout lists it under Automatic Zero.
VIOLATIONS=""
author_line=$(grep -m1 -E '^[[:space:]]*Author\(s\):' "$SOURCE_FILE" | sed -E 's/^[[:space:]]*Author\(s\):[[:space:]]*//')
if [ -z "$author_line" ]; then
    VIOLATIONS="parser.c has no Author(s): line with a name on it"
elif printf '%s' "$author_line" | grep -qF '<Your Name Here>'; then
    VIOLATIONS="the Author(s): line of parser.c still reads <Your Name Here>"
fi

echo ""
echo "   Code requirements:"
if [ -z "$VIOLATIONS" ]; then
    print_status "$GREEN" "     file name correct, compiles with the course flags, author set"
    echo "     The instructor checks the rest of the header by reading parser.c."
    FINAL_SCORE="$SCORE"
else
    print_status "$RED" "     $VIOLATIONS"
    FINAL_SCORE="0.00"
fi
[ -n "$LEX_PROBLEM" ] && print_status "$YELLOW" "     pipeline cases: $LEX_PROBLEM"

echo ""
echo "============================================================"
if [ -z "$VIOLATIONS" ]; then
    print_status "$SUMMARY_COLOR" "   FINAL SCORE: ${FINAL_SCORE} / ${TOTAL_POINTS}"
else
    print_status "$RED" "   FINAL SCORE: 0.00 / ${TOTAL_POINTS}"
    echo "   The test score above does not apply: an Author(s): line that is"
    echo "   missing or left at the placeholder is an automatic zero."
fi
echo "============================================================"

rm -f -- "$SOURCE_BASENAME" "$LEX_BASENAME"
if [ $FAILED_TESTS -ne 0 ]; then
    echo ""
    echo "   Cases that failed:"
    for n in $FAILED_NAMES; do echo "     $n"; done
    echo ""
    echo "   Details for every failed case are in:"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/info.txt"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/diff.txt"
    echo ""
    echo "   Lines beginning with \"<\" are yours. Lines beginning with \">\" are expected."
    exit 1
fi
exit 0
