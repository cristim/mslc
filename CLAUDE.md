@AGENTS.md

# Claude Code rules for mslc

These rules apply on top of AGENTS.md. Where they differ, these win for work done in this repository by the owner's agents.

## Landing changes

- Commit straight to `master`. Do not open pull requests for owner work. Rebase onto `origin/master` and fast-forward push. Never force-push.
- Each commit names the issues it touches: `Refs #n` while acceptance is incomplete, `Closes #n` only when every acceptance point is verified.
- Keep each commit at or under 400 changed lines. Split larger work into several commits.
- Conventional-commit subjects. End every commit message with `Co-Authored-By: Claude Haiku 5.5 <noreply@anthropic.com>`.
- Write commit messages with `git commit -F <file>`, never `-m` with a heredoc. No `--no-verify`.
- Watch CI after every push. A red `master` is fixed first, before other work.

## Review gate

- Before a commit lands, two adversarial review passes by the same independent reviewer. The reviewer did not write the change, treats the author's claims as hypotheses, and tries to prove the change wrong.
- A review verdict applies to the exact tree reviewed. If the commit changes after review, review it again.
- A probe that a change claims to fix must fail on the parent commit and pass after.

## Verification

- Build in a fresh build directory per branch (`~/scratch/mslc/build-<n>`), never a shared one. Build every target before running `ctest`, because a stale or partial build gives misleading results.
- `ctest --test-dir <build> -j8` must pass before a push. `indium-corpus` is skipped without an indium path; say so in the report.
- Claims of Apple parity need evidence from the Apple oracle (`~/scratch/mslc/safe-metal.sh -S -emit-llvm`) on the same probe, recorded in the issue.
- `spirv-val` proves validity, not correctness. A lowering change needs a produced-value check or an Apple comparison before it is called correct.

## Probes

- One behaviour per probe in `tests/probes/`, with its expectation in the leading comment block (see AGENTS.md for the needles).
- A probe that pins a rejection pins the reject path, and its message is part of the expectation.

## Worktrees and scratch space

- Every change lives in its own worktree under `~/.claude/worktrees/`, created from `origin/master`. Never work in the main checkout for a change.
- Scratch files go under `~/scratch/mslc/`, never `/tmp`.
- Clean up a worktree only after its branch is merged and the worktree is clean. Use `git worktree remove`, never `--force`. Dry-run the reclaim script before applying it.

## Issues

- Do not close an issue until every acceptance point is verified. Otherwise comment with the evidence and the remaining gap.
- Do not close an issue that another person opened unless asked.
- Comment on an issue when a commit lands for it, naming the SHA and what remains.

## Blockers and questions

- A blocked item is held; unblocked items keep going. Do not stop the whole goal for one blocked item.
- Ask only for decisions that need the owner: a choice that changes behaviour the owner has not settled, a deletion, spending, or a production change.

## Destructive operations

- Do not run `git reset --hard`, `git branch -D`, `git filter-repo`, `git gc`, `git prune`, `git worktree prune`, or `rm -rf` on anything this session did not create, without explicit confirmation.
- Do not use `--force` on any git command.

## Prose

- No em dashes in commit messages, comments, issue text or documentation.
