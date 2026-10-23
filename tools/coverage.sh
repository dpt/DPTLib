#!/bin/sh
# coverage.sh -- build DPTLibTest with coverage, run it, report per-file
#
# Usage (from the repo root): tools/coverage.sh [test names...]
# Writes build-coverage/coverage/index.html; prints a summary to stdout.

set -e

dir=build-coverage

if command -v xcrun >/dev/null 2>&1; then
  cov="xcrun llvm-cov"
  profdata="xcrun llvm-profdata"
else
  cov=llvm-cov
  profdata=llvm-profdata
fi

if [ ! -f $dir/CMakeCache.txt ]; then
  cmake -B $dir -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
    -DBUILD_TESTS=YES -DUSE_COVERAGE=YES
fi
cmake --build $dir --target DPTLibTest

rm -f $dir/*.profraw
# A failing test still yields a profile: report it, then exit with its status
status=0
LLVM_PROFILE_FILE=$dir/test.profraw ./$dir/DPTLibTest-debug -resources . "$@" ||
  status=$?
$profdata merge -sparse $dir/test.profraw -o $dir/test.profdata

# Report library code only: skip tests, apps and vendored code
ignore='(/test/|/apps/|/fortify/|/_deps/)'
$cov report ./$dir/DPTLibTest-debug -instr-profile=$dir/test.profdata \
  -ignore-filename-regex="$ignore"
$cov show ./$dir/DPTLibTest-debug -instr-profile=$dir/test.profdata \
  -ignore-filename-regex="$ignore" -format=html -output-dir=$dir/coverage
echo "HTML report: $dir/coverage/index.html"
exit $status
