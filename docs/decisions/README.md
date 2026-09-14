# Decisions

Architecture decision records: the *why* behind a choice that outlives the
work that made it, when that why lives in none of the three places that
already carry it.

## When to write one

Three places already record decisions, and each has a job:

| place | carries |
|---|---|
| the code comment | the mechanism and an item id, one to three lines |
| `docs/components/*/technical-reference.md` | what the mechanism is, described as an initial specification would — no history |
| `docs/CODE-ROADMAP.md` and the commit | the narrative, the figures, what was refuted, what was measured |

A record here is written only when someone reading the code in a year will
ask *why* and find the answer in none of them: a constraint that shaped the
design and is not visible in it, an alternative that was measured and
rejected, a limit that is accepted rather than fixed. One decision per
file. Not every lot produces one; most do not.

The `.specify/memory/constitution.md` holds the standing principles. A record
here applies them to one case; it does not restate them.

## Format

`NNNN-short-slug.md`, numbered in order of writing. Thirty lines is the
target; a record that needs more is usually two.

```markdown
# NNNN — Title, as a statement of the decision

**Status**: accepted | superseded by NNNN
**Date**: YYYY-MM-DD

## Context
What was true, what was measured, what constrained the choice.

## Decision
What was chosen, in one paragraph.

## Consequences
What this makes easy, what it makes impossible, what it costs, what it
leaves open.
```

## Working documents

The specifications, plans and reviews a lot is built from are scaffolding:
they live outside the tree (`_bmad-output/`, ignored) and end with the pull
request. What survives them is the reference, the roadmap entry, the commit
message — and, when the test above says so, a record here. The ones that
were committed before this rule are in the history:
`git log --all -- _bmad-output/`.

## Index

*(none yet)*
