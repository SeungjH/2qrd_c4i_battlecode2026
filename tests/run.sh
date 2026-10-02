#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_dir"
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/battlecode-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-clang++}" -std=c++20 -O2 -Wall -Wextra main.cpp -o "$test_dir/bot"
"${CXX:-clang++}" -std=c++20 -O2 -Wall -Wextra tests/strategy_tests.cpp -o "$test_dir/strategy_tests"
for case_name in short_loop warnings escape donation contested receiver late_donation hunt_join hunt_timeout split_radio reverse_rescue roles_and_income productive_loop head_trade early_support portal_route trap_share queen_only queen_relay warning_queue wide_scout pocket_profit; do
    "$test_dir/strategy_tests" "$case_name"
done
python3 tests/test_behavior.py "$test_dir/bot"
