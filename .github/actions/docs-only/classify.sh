#!/usr/bin/env bash
# Writes code=false to $GITHUB_OUTPUT when every file the pull request changes
# is documentation (*.md anywhere, or under docs/), else code=true. HEAD is the
# pull request's merge commit, HEAD^1 the base branch. A push ($EVENT other than
# pull_request), or a diff that cannot be read, gives code=true.
set -euo pipefail
code=true
if [ "${EVENT:-}" = pull_request ]; then
  # --no-renames: a source renamed to a .md file lists both names.
  if files=$(git diff --no-renames --name-only HEAD^1 HEAD); then
    [ -n "$files" ] && code=false
    while IFS= read -r file; do
      [ -z "$file" ] && continue
      case "$file" in
        *.md|docs/*) ;;
        *) code=true; echo "code changed: $file"; break ;;
      esac
    done <<< "$files"
  else
    echo "::warning::the changed files cannot be listed; every job runs"
  fi
fi
if [ "$code" = false ]; then echo "documentation only: the builds and tests are skipped"; fi
echo "code=$code" >> "$GITHUB_OUTPUT"
