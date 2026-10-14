#!/usr/bin/env bash
# Format (default) or check (--check) all of our own C code with the pinned clang-format.
# Install: pip install clang-format==19.1.7
set -euo pipefail
cd "$(dirname "$0")/.."
WANT=19.1.7
CF=${CLANG_FORMAT:-clang-format}
ver=$("$CF" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
[ "$ver" = "$WANT" ] || { echo "need clang-format $WANT (pip install clang-format==$WANT), found $ver" >&2; exit 2; }
# Ours: main/, components/, test/host, test/hootl. Generated/vendor files excluded.
mapfile -t files < <(git ls-files -- 'main/*.[ch]' 'components/*.[ch]' 'test/*.[ch]' \
    ':!:components/display/font_data.c' ':!:**/managed_components/**' ':!:**/build*/**')
if [ "${1:-}" = "--check" ]; then
    "$CF" --style=file --dry-run -Werror "${files[@]}"
    echo "format: ${#files[@]} files clean"
else
    "$CF" --style=file -i "${files[@]}"
    echo "format: ${#files[@]} files formatted"
fi
