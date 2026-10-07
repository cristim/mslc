# Working agreement

Several agents work this repository at once, from different machines, each pushing its
own branches and opening its own PRs. There is no scheduler and no shared context
between sessions, so this file is the coordination surface. Read it before starting
anything.

It is deliberately specific to this repository. Generic advice about writing good
commits is not here; what is here is the set of ways work in `mslc` has actually
collided, and the rules that prevent each one.

## Check before you start

Every capability gets a GitHub issue. Before you begin one:

```sh
gh issue list --state open
gh pr list --state open
```

Read both, plus any open issue that names a file you are about to change. If an
open PR already touches `src/sema.cpp` for the thing you want to build, you have
found the collision before you wrote the code rather than after.

An issue you did not write may be stale. Say so in a comment and take it, rather
than leaving it claimed and unstarted.

## One capability, one issue, one branch, one PR

- The **issue** is the shared record. It is what the other machines read to learn
  what is in flight, so it gets updated when you start, and again when you land or
  abandon.
- The **branch** is named after the issue number: `feat/23-multiple-entry-points`.
  The linkage is then unambiguous from `git branch`, with no archaeology.
- The **PR** is one capability. A PR that fixes a review and adds a feature is two
  capabilities; split it.

Atomic commits and honest manifests are already the norm here. Keep them: one
commit per coherent step, conventional-commit subject, and no commit whose message
claims more than the diff does.

## Overlap

Someone else's PR is in the same area as yours. In that order:

1. **Read their diff first.** `gh pr diff <n>`. Understand what they changed and
   why before deciding anything.
2. **Build on their branch** if their work is a prerequisite or shares the same
   files. Say in the issue which PR you built on and why.
3. **Narrow your scope to a disjoint area** if it can be done honestly. Say in the
   issue which files you are taking and which you are leaving, so nobody spends a
   session rediscovering that.
4. **Stop and comment** if neither works. Comment on their PR and yours, with the
   specific overlap, and let a human decide.

Never silently duplicate work. If two PRs implement the same capability, one of
them is wasted and its author does not know it.

Never resolve a conflict with someone else's in-flight work unilaterally. That
means: do not rebase, reset, force-push, or edit a branch that is not yours, and do
not merge a PR from another session's stack without saying so in the issue first.

## Git rules

- Never force-push `master`.
- Never rebase or reset a branch that is not yours. A force-push to a branch with
  an open PR destroys the review that is in progress on it.
- If `master` moved under you, rebase onto it and **re-measure** before claiming
  anything. The corpus numbers in your PR description were true of the old base.
- Never `git gc`, `prune`, `repack`, or `worktree prune` in a repository other
  sessions are using. Those reach into shared state.
- Scratch space goes on disk, not in `/tmp`, which is shared by every agent on the
  machine and has filled twice: a truncated build there reads as a wrong answer
  rather than a full disk. Use a fresh build directory per branch; a stale
  `mslc-corpus` binary has produced a wrong reading more than once.
- The indium checkout the corpus points into is **read-only**. New harnesses go
  under the scratch directory and are not committed to any repository.

## Measurement is the deliverable

The subset is defined by measurement, not by intention, so a PR that does not state
its numbers has not stated its result.

Every PR says the corpus count **before and after**, from a fresh build:

```sh
cmake -B <fresh-dir> -S . -DCMAKE_BUILD_TYPE=Release \
  -DMSLC_INDIUM_DIR=<path-to-indium>
cmake --build <fresh-dir> --parallel
ctest --test-dir <fresh-dir>
<fresh-dir>/mslc-corpus tests/indium_corpus.txt --root <path-to-indium> --spirv-val "$(command -v spirv-val)"
```

`tests/corpus.txt` is the Metal Blender 4.5.14 actually calls through
`newLibraryWithSource:`, frozen verbatim. `tests/indium_corpus.txt` is indium's own
test corpus, and it is the one that moves as the port progresses. Both are reported
as "N of M reach spirv-val-valid SPIR-V".

A capability that moves a diagnostic but not a count is still worth landing. Say
so plainly: "1 of 7 before, 1 of 7 after; `test/cube` now parses in full and stops
at the multi-entry-point check instead of at the attribute list." That is a real
result. Overclaiming it as progress on the corpus is not.

## Review and merge evidence

Use a capable independent reviewer; no fixed model is required. For code changes,
exercise the actual path and data shape with realistic fixtures, mocks, recorded
data or local integration. Show a failure before the fix and a pass after it where
applicable, run a fresh build and relevant suite, and state coverage gaps. A live
cloud account or purchase is not a gate. These methods complement the corpus counts
and produced-value read-backs; they do not replace them.

The standing owner waiver permits normal merge when CodeRabbit is demonstrably
throttled or out of quota and the remaining gates pass. Record
`CR waived: quota, adversarial review + local verification + green CI` on the PR.
A green "Review rate limited" status is not a review. Resolve every available finding,
obtain independent review and local proof, then check fresh CI for the exact live
head and live mergeability immediately before a normal merge. Preserve branch
protections. Retain one tracked retrospective full-review retry, with its owner
and quota-reset time, and verify there is no duplicate. Address later actionable
findings in a focused follow-up PR.

## Tests and manifests

One probe per behaviour, in `tests/probes/`, each carrying its own expectation in a
leading comment block:

| Needle | Meaning |
| --- | --- |
| `EXPECT: valid` | compiles, and the module passes `spirv-val --target-env vulkan1.3` |
| `EXPECT: error <text>` | compilation fails and the diagnostic contains `<text>` |
| `DISASM: <text>` | the disassembly contains `<text>` |
| `DISASM-NOT: <text>` | the disassembly does not contain `<text>` |
| `DISASM-MATCH: <regex>` / `DISASM-NO-MATCH: <regex>` | as above, by regex |
| `REFLECT: <text>` / `REFLECT-NOT: <text>` | the reflection JSON does or does not contain `<text>` |

A probe that pins a rejection pins the **reject** path. If a change only lands the
parse and the accept path needs a later change to become interface decorations,
say so in the PR: the probe is not evidence the feature works, only that the
unsupported case is diagnosed rather than miscompiled.

## `spirv-val` proves validity, not correctness

A wrong layout is `spirv-val`-valid and silently wrong. That is the failure mode
this project has actually hit: a `float3` laid out as 12 bytes instead of 16
validated, ran, and read bytes the host had not written.

So where a construct's correctness can be checked against produced values, check it
against produced values: run the kernel on lavapipe, read the buffer back, and
compare floats to a host reference. Harnesses for that live in the scratch
directory, keyed by feature (`matrix-readback.c`, `builtin-readback.c`,
`layout-readback.c`). They are not committed.

Two traps in that harness, both found the hard way:

- A NaN passes a tolerance test, because every comparison against a NaN is false.
  Assert the value is not NaN before comparing it.
- `pow` with a negative base is undefined in GLSL. A harness that calls it that
  way measures nothing.

Keep a negative control that fires. A read-back harness that cannot fail is not
evidence.

## What `master` already gets right

Do not regress these. They are where `master` is better than the older reference
branch, and a port that copies the reference will undo them:

- SPIR-V 1.5.
- Execution modes and control masks generated from the grammar.
- `PhysicalStorageBuffer64` addressing with a binding-0 address block, which is
  what indium actually binds. The reference branch's `Logical` addressing with
  per-buffer descriptors does not match indium.
- The descriptor set is stage-dependent: indium builds set 0 from the vertex
  function and set 1 from the fragment function.

## Known traps in the reference branch

`agent-metal-gaps-work` is readable with `git show agent-metal-gaps-work:<path>` and
is useful for **capability**, not for emitter design. Its `src/` layout matches
`master`'s. Do not port these parts:

- Its constant folder leaves `% 0` unguarded, which traps on the host.
- Its `foldInitializer` bool checks cannot fire as written.
- It assigns a type id to a field expecting a type.
