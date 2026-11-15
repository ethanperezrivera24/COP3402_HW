#!/bin/bash

# COP 3402 HW4 - PL/0 compiler (parser, symbol table, code generator) testing script
# Author: Jie Lin, Ph.D.
# Affiliation: University of Central Florida
# Course: COP 3402 - Systems Software
#
# Compiles and tests a student's parsercodegen.c, and their HW2 lex.c and HW1
# vm.c for the pipeline cases. C only.
#
# Usage, from inside the unzipped parsercodegen_test folder:
#   ./test_parsercodegen.sh parsercodegen.c lex.c vm.c [--quiet]
# All three files are required arguments, in that order; each may be a path.
# A lex.c or vm.c that is missing, misnamed, or does not compile costs only
# the pipeline cases.
#
# Every test case is one line of test_cases/cases.txt and runs in its own
# empty working directory, holding only what the case's mode provides:
#
#   files        tokens.txt and nametable.txt, copied from the case's posted
#                <case>.tokens.txt and <case>.nametable.txt; then ./parsercodegen
#   names-only   only nametable.txt; then ./parsercodegen
#   tokens-only  only tokens.txt; then ./parsercodegen
#   pipeline     only the source program <case>.txt; then your ./lex <case>.txt
#                writes the two files, ./parsercodegen reads them and writes
#                elf.txt, and your ./vm elf.txt runs it, reading <case>.stdin
#                when the case posts one
#
# A compiler case earns its points only when ALL of the following hold:
#
#   1. the exit status is what the manifest requires: 0, or for an error
#      case a status from 1 to 123 (a crash or a timeout is not an error
#      report);
#   2. the compiler's standard output matches expected_results/<case>.txt;
#   3. for a case whose exit status must be 0, the elf.txt it wrote matches
#      expected_results/<case>.elf.txt. In an error case elf.txt is not read.
#
# A pipeline case earns its points only when your lex, your parsercodegen and
# your vm each exit with status 0 and the VM's standard output matches
# expected_results/<case>.txt.
#
# The cases are worth 95 points. The FINAL SCORE, out of 100, adds the
# reflection's 5 points, which the instructor deducts in Canvas when the
# reflection is missing or is not about this homework.
#
# Comparison ignores blank lines and the amount of spacing: each run of
# spaces and tabs counts as one space, and spacing at either end of a line is
# ignored. The text, its order, and the space between two values must match.
# Your columns do not have to line up with the reference's; the values in
# them do.
#
# Each run of a program gets a time limit, a forced kill a few seconds after
# the limit for a program that ignores the first signal, a cap on how much it
# may write, and an empty standard input (the pipeline's vm reads the case's
# .stdin file instead, when there is one).
#
# The whole run of all the cases is limited to 15 minutes, so a program that
# loops or crawls cannot hold a grader for longer. A case still running when
# the limit is reached is stopped and fails, and every case after it is not
# run. Those cases score 0; the cases already finished keep their points.

# Configuration
SOURCE_BASENAME="parsercodegen"
DEFAULT_C_SOURCE="$SOURCE_BASENAME.c"
LEX_BASENAME="lex"
DEFAULT_LEX_SOURCE="$LEX_BASENAME.c"
VM_BASENAME="vm"
DEFAULT_VM_SOURCE="$VM_BASENAME.c"
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
                        # largest expected result is under 10 KB
TOTAL_TIME_LIMIT_SEC=900  # the limit on running all the cases together
TOTAL_LIMIT_MIN=$(( TOTAL_TIME_LIMIT_SEC / 60 ))
CFLAGS="-Wall -Wextra -std=c11 -O2"

CASE_POINTS=95          # the cases of the manifest
REFLECTION_POINTS=5     # credited here; deducted in Canvas when the
                        # reflection is missing or not about this homework
TOTAL_POINTS=$(( CASE_POINTS + REFLECTION_POINTS ))
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

# Two files match when they differ only in blank lines and in the amount of
# spacing: each run of spaces and tabs counts as one space, and spacing at
# either end of a line is ignored. A space between two values cannot be
# dropped, so "7 02 03" and "70203" do not match "7 0 203". diff -w would
# accept both, because it removes every space before comparing.
squeeze() { sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//' "$1"; }
same_text() { diff -a -B <(squeeze "$1") <(squeeze "$2"); }

case_purpose() {
    [ -f "$PURPOSES" ] || return 0
    awk -F'\t' -v want="$1" '$1 == want { print $2; exit }' "$PURPOSES"
}

category_name() {
    case "$1" in
        a) echo "Expressions, assignment, read and write" ;;
        b) echo "If, else and while" ;;
        s) echo "Procedures, calls and levels" ;;
        g) echo "Syntax errors of procedure and call" ;;
        y) echo "Duplicate, undeclared, and kind errors" ;;
        l) echo "Entry limit, code limit, missing files" ;;
        p) echo "Pipeline: your lex, compiler, and vm" ;;
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
    echo "  The name is graded: a parsercodegen.c under any other name scores 0," >&2
    echo "  even when the program is correct." >&2
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

if [ ${#FILES[@]} -ne 3 ]; then
    echo "Usage: $0 parsercodegen.c lex.c vm.c [--quiet]"
    echo "Give all three files: your compiler first, then your HW2 scanner, then"
    echo "your HW1 virtual machine. Each may be a path."
    echo "They must be named exactly $DEFAULT_C_SOURCE, $DEFAULT_LEX_SOURCE and $DEFAULT_VM_SOURCE."
    echo ""
    echo "This script will:"
    echo "  1. Compile parsercodegen.c, lex.c and vm.c with the course flags"
    echo "  2. Run every test case listed in $MANIFEST"
    echo "  3. Check each exit status and compare each output and each elf.txt"
    echo "  4. Report your score out of $TOTAL_POINTS ($CASE_POINTS from the cases, $REFLECTION_POINTS for the"
    echo "     reflection) and show any differences"
    exit 1
fi
SOURCE_FILE=${FILES[0]}
LEX_FILE=${FILES[1]}
VM_FILE=${FILES[2]}
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
    echo "A parsercodegen.c that does not compile on Eustis scores 0. Fix the errors and run this again."
    exit 1
fi
print_status "$GREEN" "PASS: Compilation of $SOURCE_FILE succeeded"
EXE="$PWD/$SOURCE_BASENAME"

# lex.c and vm.c are needed only by the pipeline cases. Without either, those
# cases score 0 and every other case still runs. compile_helper prints the
# executable's path, or nothing and a reason on standard error.
compile_helper() {
    local file=$1 base=$2 required=$3 what=$4
    if [ ! -f "$file" ]; then
        echo "$file not found" >&2
        return 1
    fi
    if ! file=$(resolve_name "$file" "$base" "$required"); then
        echo "$required has the wrong file name" >&2
        return 1
    fi
    say_status "$BLUE" "INFO: Compiling $file (your $what, for the pipeline cases)" >&2
    rm -f -- "$base"
    run_with_timeout "$COMPILE_TIMEOUT_SEC" gcc $CFLAGS "$file" -o "$base" >&2
    if [ $? -eq 0 ] && [ -x "$base" ]; then
        print_status "$GREEN" "PASS: Compilation of $file succeeded" >&2
        echo "$PWD/$base"
        return 0
    fi
    echo "$file does not compile" >&2
    return 1
}
PIPE_PROBLEM=""
LEX_EXE=$(compile_helper "$LEX_FILE" "$LEX_BASENAME" "$DEFAULT_LEX_SOURCE" "HW2 scanner" 2> "$PWD/.lex_problem")
[ -z "$LEX_EXE" ] && PIPE_PROBLEM="$(grep -v -e '^INFO' -e '^PASS' -e 'INFO:' -e 'PASS:' "$PWD/.lex_problem" | tail -1)"
VM_EXE=$(compile_helper "$VM_FILE" "$VM_BASENAME" "$DEFAULT_VM_SOURCE" "HW1 virtual machine" 2> "$PWD/.vm_problem")
if [ -z "$VM_EXE" ]; then
    vm_problem="$(grep -v -e 'INFO:' -e 'PASS:' "$PWD/.vm_problem" | tail -1)"
    PIPE_PROBLEM="${PIPE_PROBLEM:+$PIPE_PROBLEM; }$vm_problem"
fi
[ "$QUIET" -eq 0 ] && cat "$PWD/.lex_problem" "$PWD/.vm_problem" | grep -e 'INFO:' -e 'PASS:'
rm -f "$PWD/.lex_problem" "$PWD/.vm_problem"
if [ -n "$PIPE_PROBLEM" ]; then
    print_status "$YELLOW" "WARN: $PIPE_PROBLEM; the pipeline (p) cases will score 0."
fi
say ""

rm -rf "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"
mkdir -p "$STUDENT_OUTPUT_DIR" "$FAILED_OUTPUT_DIR"
SCRATCH=$(mktemp -d 2>/dev/null || mktemp -d -t parsercodegen_test)
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
    local name=$1 reason=$2 expected=$3 output=$4 elf=$5 elf_expected=$6
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
        [ -n "$elf_expected" ] && echo "Expected elf.txt: $elf_expected"
        [ -n "$elf" ] && echo "Your elf.txt: $elf"
    } > "$dir/info.txt"
    if [ -f "$output" ] && [ -f "$expected" ]; then
        same_text "$output" "$expected" > "$dir/diff.txt" 2>/dev/null
    fi
    if [ -n "$elf" ] && [ -f "$elf" ] && [ -f "$elf_expected" ]; then
        same_text "$elf" "$elf_expected" > "$dir/elf_diff.txt" 2>/dev/null
    fi
}

# Runs one program in a case's working directory with the time limit, the
# forced kill, and the output cap. Standard input is the file given, usually
# /dev/null. The subshell keeps the cap away from this script.
run_capped() {
    local dir=$1 out=$2 err=$3 limit=$4 input=$5
    shift 5
    ( ulimit -S -f "$OUTPUT_LIMIT_KB" 2>/dev/null
      cd "$dir" && run_with_timeout "$limit" "$@" ) < "$input" > "$out" 2> "$err"
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
    local elf_expected="$EXPECTED_DIR/$name.elf.txt"
    local output="$STUDENT_OUTPUT_DIR/${name}_output.txt"
    local elf_output="$STUDENT_OUTPUT_DIR/${name}_elf.txt"
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
        if [ "$mode" = "pipeline" ] || [ "$want" != "0" ]; then
            record_failure "$name" "$1" "$expected" "$output" "" ""
        else
            record_failure "$name" "$1" "$expected" "$output" "$elf_output" "$elf_expected"
        fi
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_NAMES="$FAILED_NAMES $name"
    }
    pass_case() {
        EARNED=$(awk -v a="$EARNED" -v b="$points" 'BEGIN { printf "%g", a + b }')
        add_points "${prefix}_earned" "$points"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        say_status "$GREEN" "   PASS: $1"
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

    local limit status
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
            run_pipeline_case
            return ;;
        *)
            fail_case "unknown mode $mode in the manifest"
            return ;;
    esac
    say "   Command: ./parsercodegen"

    limit=$(next_limit)
    run_capped "$dir" "$output" "$dir/.stderr" "$limit" /dev/null "$EXE"
    status=$?
    [ -f "$dir/elf.txt" ] && cp "$dir/elf.txt" "$elf_output"

    if [ "$want" = "0" ] && [ $status -ne 0 ]; then
        fail_case "exit status should be 0, but $(explain_status $status "$limit")"
    elif [ "$want" != "0" ] && { [ $status -lt 1 ] || [ $status -gt 123 ]; }; then
        if [ $status -eq 0 ]; then
            fail_case "exit status should be 1 (this case holds an error on purpose), but it exited with 0"
        else
            fail_case "exit status should be 1, but $(explain_status $status "$limit")"
        fi
    elif ! same_text "$output" "$expected" > /dev/null 2>&1; then
        fail_case "output differs from $expected"
        echo "   First differences (blank lines ignored, spacing squeezed to one space):"
        same_text "$output" "$expected" | head -10
    elif [ "$want" != "0" ]; then
        pass_case "exit status correct and output matches (exit $status)"
    elif [ ! -f "$dir/elf.txt" ]; then
        fail_case "the program was accepted but no elf.txt was written"
    elif ! same_text "$elf_output" "$elf_expected" > /dev/null 2>&1; then
        fail_case "elf.txt differs from $elf_expected"
        echo "   First differences in elf.txt (blank lines ignored, spacing squeezed to one space):"
        same_text "$elf_output" "$elf_expected" | head -10
    else
        pass_case "exit status correct, output and elf.txt match (exit 0)"
    fi
}

# Your lex, then your parsercodegen, then your vm, in the case's directory.
# Uses the variables of run_test_case.
run_pipeline_case() {
    : > "$output"
    if [ -n "$PIPE_PROBLEM" ]; then
        fail_case "pipeline case not run: $PIPE_PROBLEM"
        return
    fi
    cp "$INPUTS_DIR/$name.txt" "$dir/"
    local input=/dev/null
    if [ -f "$INPUTS_DIR/$name.stdin" ]; then
        input="$PWD/$INPUTS_DIR/$name.stdin"
        say "   Command: ./lex $name.txt && ./parsercodegen && ./vm elf.txt < $name.stdin"
    else
        say "   Command: ./lex $name.txt && ./parsercodegen && ./vm elf.txt"
    fi
    local stage_output stage
    for stage in lex parsercodegen vm; do
        if [ "$(time_left)" -le 0 ]; then
            NOT_RUN=$((NOT_RUN + 1))
            fail_case "$stage not run: the whole run reached its ${TOTAL_LIMIT_MIN}-minute limit"
            return
        fi
        limit=$(next_limit)
        case $stage in
            lex)
                stage_output="$STUDENT_OUTPUT_DIR/${name}_lex_output.txt"
                run_capped "$dir" "$stage_output" /dev/null "$limit" /dev/null "$LEX_EXE" "$name.txt" ;;
            parsercodegen)
                stage_output="$STUDENT_OUTPUT_DIR/${name}_compiler_output.txt"
                run_capped "$dir" "$stage_output" /dev/null "$limit" /dev/null "$EXE"
                status=$?
                [ -f "$dir/elf.txt" ] && cp "$dir/elf.txt" "$elf_output"
                (exit $status) ;;
            vm)
                stage_output="$output"
                run_capped "$dir" "$stage_output" /dev/null "$limit" "$input" "$VM_EXE" elf.txt ;;
        esac
        status=$?
        if [ $status -ne 0 ]; then
            fail_case "your $stage failed: $(explain_status $status "$limit"); its output is in $stage_output"
            return
        fi
    done
    if same_text "$output" "$expected" > /dev/null 2>&1; then
        pass_case "all three exited 0 and the VM output matches"
    else
        fail_case "VM output differs from $expected"
        echo "   First differences (blank lines ignored, spacing squeezed to one space):"
        same_text "$output" "$expected" | head -10
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

CASE_SCORE=$(awk -v a="$EARNED" 'BEGIN { printf "%.2f", a }')
SCORE=$(awk -v a="$EARNED" -v r="$REFLECTION_POINTS" 'BEGIN { printf "%.2f", a + r }')
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
            printf '     %-42s %6s / %-6s\n' "$(category_name "$prefix")" "$earn" "$poss"
            ;;
    esac
done
printf '     %-42s %6s / %-6s\n' "Reflection (deducted in Canvas if missing)" "$REFLECTION_POINTS" "$REFLECTION_POINTS"
print_status "$SUMMARY_COLOR" "   HW4 test score: ${CASE_SCORE} / ${CASE_POINTS}, plus the reflection's ${REFLECTION_POINTS}: ${SCORE} / ${TOTAL_POINTS}"

# The rule a test cannot see: the handout lists it under Automatic Zero.
VIOLATIONS=""
author_line=$(grep -m1 -E '^[[:space:]]*Author\(s\):' "$SOURCE_FILE" | sed -E 's/^[[:space:]]*Author\(s\):[[:space:]]*//')
if [ -z "$author_line" ]; then
    VIOLATIONS="parsercodegen.c has no Author(s): line with a name on it"
elif printf '%s' "$author_line" | grep -qF '<Your Name Here>'; then
    VIOLATIONS="the Author(s): line of parsercodegen.c still reads <Your Name Here>"
fi

echo ""
echo "   Code requirements:"
if [ -z "$VIOLATIONS" ]; then
    print_status "$GREEN" "     file name correct, compiles with the course flags, author set"
    echo "     The instructor checks the rest of the header by reading parsercodegen.c."
    FINAL_SCORE="$SCORE"
else
    print_status "$RED" "     $VIOLATIONS"
    FINAL_SCORE="0.00"
fi
[ -n "$PIPE_PROBLEM" ] && print_status "$YELLOW" "     pipeline cases: $PIPE_PROBLEM"

echo ""
echo "============================================================"
if [ -z "$VIOLATIONS" ]; then
    print_status "$SUMMARY_COLOR" "   FINAL SCORE: ${FINAL_SCORE} / ${TOTAL_POINTS}"
else
    print_status "$RED" "   FINAL SCORE: 0.00 / ${TOTAL_POINTS}"
    echo "   The test score above does not apply: an Author(s): line that is"
    echo "   missing or left at the placeholder is an automatic zero."
fi
echo "   The score includes the reflection's ${REFLECTION_POINTS} points. The instructor"
echo "   deducts them in Canvas when the reflection is missing or is not"
echo "   about this homework."
echo "============================================================"

rm -f -- "$SOURCE_BASENAME" "$LEX_BASENAME" "$VM_BASENAME"
if [ $FAILED_TESTS -ne 0 ]; then
    echo ""
    echo "   Cases that failed:"
    for n in $FAILED_NAMES; do echo "     $n"; done
    echo ""
    echo "   Details for every failed case are in:"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/info.txt"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/diff.txt"
    echo "     $FAILED_OUTPUT_DIR/<case_name>/elf_diff.txt   (when elf.txt differs)"
    echo ""
    echo "   Lines beginning with \"<\" are yours. Lines beginning with \">\" are expected."
    exit 1
fi
exit 0
