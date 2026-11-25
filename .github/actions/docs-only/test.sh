#!/usr/bin/env bash
# The cases classify.sh must get right, each on a throwaway repository: a base
# commit, then a "merge" commit on top holding the change.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export GIT_AUTHOR_NAME=test GIT_AUTHOR_EMAIL=test@example.invalid
export GIT_COMMITTER_NAME=test GIT_COMMITTER_EMAIL=test@example.invalid
failed=0

# check <name> <event> <expected> <shell command changing the tree>
check() {
  local repo="$work/$1" out
  git init -q "$repo"
  (
    cd "$repo"
    mkdir -p docs src
    echo a > README.md; echo a > src/a.cpp; echo a > docs/Doxyfile; echo a > library.json
    mkdir -p docs/c Comp/test/t examples/Ex
    echo a > docs/c/read.md; echo a > docs/c/free.md; echo a > examples/README.md
    echo 'readRepoFile("docs/c/read.md");' > Comp/test/t/t.cpp
    git add -A
    base=$(git commit-tree "$(git write-tree)" -m base)
    git reset -q "$base"
    eval "$4"
    git add -A
    head=$(git commit-tree "$(git write-tree)" -p "$base" -m change)
    git reset -q --hard "$head"
  )
  out=$(mktemp -p "$work")
  (cd "$repo" && EVENT=$2 GITHUB_OUTPUT=$out bash "$here/classify.sh" > /dev/null)
  if [ "$(cat "$out")" = "code=$3" ]; then
    echo "ok   $1"
  else
    echo "FAIL $1: want code=$3, got $(cat "$out")"; failed=1
  fi
}

check docs-only        pull_request false 'echo b >> README.md; echo b >> docs/Doxyfile'
check nested-md        pull_request false 'mkdir -p src/x; echo b > src/x/notes.md'
check code-only        pull_request true  'echo b >> src/a.cpp'
check manifest         pull_request true  'echo b >> library.json'
check mixed            pull_request true  'echo b >> README.md; echo b >> src/a.cpp'
check rename-to-md     pull_request true  'git mv src/a.cpp src/a.md'
check docs-dir-nested  pull_request true  'mkdir -p x/docs; echo b > x/docs/f.h'
check docs-on-push     push         true  'echo b >> README.md'
check doc-read-by-test pull_request true  'echo b >> docs/c/read.md'
check doc-not-read     pull_request false 'echo b >> docs/c/free.md'
check doc-added        pull_request false 'echo b > docs/c/new.md'
check doc-deleted      pull_request true  'git rm -q docs/c/free.md'
check doc-renamed      pull_request true  'git mv docs/c/free.md docs/c/moved.md'
check examples-readme  pull_request true  'echo b >> examples/README.md'
exit $failed
