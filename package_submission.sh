#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repo_dir"

# Package only the committed bot; refuse to silently omit local source edits.
if ! git diff --quiet HEAD -- main.cpp bot.toml; then
    printf '%s\n' 'Commit changes to main.cpp and bot.toml before packaging.' >&2
    exit 1
fi

revision=$(git rev-parse --short=12 HEAD)
mkdir -p dist
archive="dist/submission-${revision}.zip"
git archive --format=zip --output="$archive" HEAD -- main.cpp bot.toml
printf 'Submission ZIP: %s/%s\n' "$repo_dir" "$archive"
