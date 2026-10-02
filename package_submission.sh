#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repo_dir"

# Package only the committed bot; refuse to silently omit local source edits.
if ! git diff --quiet HEAD -- main.cpp bot.toml; then
    printf '%s\n' 'Commit changes to main.cpp and bot.toml before packaging.' >&2
    exit 1
fi

version=$(sed -n '1s/^# Explorer \(v[0-9][0-9]*\).*/\1/p' README.md)
if [ -z "$version" ]; then
    printf '%s\n' 'Version not found in README.md.' >&2
    exit 1
fi
mkdir -p dist
archive="dist/2qrd_${version}.zip"
git archive --format=zip --output="$archive" HEAD -- main.cpp bot.toml
archive_bytes=$(wc -c < "$archive" | tr -d ' ')
if [ "$archive_bytes" -ge 4000000 ]; then
    printf '%s\n' 'Submission ZIP must be smaller than 4 MB.' >&2
    rm -f "$archive"
    exit 1
fi
printf 'Submission ZIP: %s/%s\n' "$repo_dir" "$archive"
