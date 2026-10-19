#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
for name in test_hunt test_adapter; do
    "${CXX:-clang++}" -std=c++20 -O2 "$here/$name.cpp" -o "$build/$name"
    "$build/$name"
done
"${CXX:-clang++}" -std=c++20 -O2 "$here/../candidate/main.cpp" -o "$build/bot"
echo 'RL archive build and regression checks passed'
