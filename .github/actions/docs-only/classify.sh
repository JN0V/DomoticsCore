#!/usr/bin/env bash
# Writes code=false to $GITHUB_OUTPUT when every file the pull request changes
# is documentation, else code=true. HEAD is the pull request's merge commit,
# HEAD^1 the base branch. A file is documentation when it is added or modified
# (not deleted, not renamed), is *.md or under docs/, is not under examples/,
# and no test names its path: a test that reads a doc makes it code. A push
# ($EVENT other than pull_request), or a diff that cannot be read, gives
# code=true.
set -euo pipefail
code=true
if [ "${EVENT:-}" = pull_request ]; then
  # --no-renames: a rename lists as a deletion and an addition.
  if changes=$(git diff --no-renames --name-status HEAD^1 HEAD); then
    [ -n "$changes" ] && code=false
    while IFS=$'\t' read -r status file; do
      [ -z "$file" ] && continue
      why=""
      case "$status" in A|M) ;; *) why="deleted or renamed" ;; esac
      if [ -z "$why" ]; then
        case "$file" in
          examples/*|*/examples/*) why="under examples/" ;;
          *.md|docs/*) ;;
          *) why="not documentation" ;;
        esac
      fi
      if [ -z "$why" ] && git grep -qF -- "$file" -- \
          ':(glob)**/test/**' ':(glob)**/tests/**' ':(glob)**/webui_test/**'; then
        why="read by a test"
      fi
      if [ -n "$why" ]; then
        code=true; echo "code changed: $file ($why)"; break
      fi
    done <<< "$changes"
  else
    echo "::warning::the changed files cannot be listed; every job runs"
  fi
fi
if [ "$code" = false ]; then echo "documentation only: the builds and tests are skipped"; fi
echo "code=$code" >> "$GITHUB_OUTPUT"
