# diffcheck: Apple's compiler against mslc, by execution

`spirv-val` proves a module is valid, not that it does what the Metal source says.
diffcheck compiles a kernel with Apple's compiler and runs it on the Apple GPU, runs
mslc's SPIR-V for the same source on lavapipe, feeds both the same seeded bytes, and
reports any output difference.

## One command

```sh
python3 tools/diffcheck/diffcheck.py file.metal [--entry k] [--grid 64] [--local 8] \
    [--seed 1] [--cases 3] [--sentinel] [--poison-padding] [--report out.json]
python3 tools/diffcheck/diffcheck.py --corpus list.txt [--root dir] [--jobs 4] [--report out.json]
```

Needs: macOS with a Metal device, `mslc` (`MSLC=path` or `--mslc`), `spirv-val`, a Vulkan
loader and Mesa's lavapipe (`brew install mesa vulkan-loader`; the ICD is found under
`$HOMEBREW_PREFIX/share/vulkan/icd.d` or taken from `VK_ICD_FILENAMES`, and only a
lavapipe manifest is accepted), `cc` and `clang++`. The two hosts are built on demand into
`$TMPDIR/diffcheck-hosts`. Nothing else outside this repository is needed.

A corpus list has one path per line, `#` comments, and an optional `std=3.1`. Paths are
relative to `--root` (default: the list's directory), so third-party shaders stay out of
the repository: the harness takes any path.

Exit status: `0` every compared case matched; `1` a mismatch, a failed negative control, a
zero-output run or a reflection disagreement; `2` a harness error; `3` nothing could be
compared (skipped, with the reason); `77` Metal or lavapipe is not available here.
In `--corpus` mode skips do not fail the run.

## What it does for a kernel

1. mslc compiles the file (`--stage kernel`) and `spirv-val --target-env vulkan1.3` must pass.
2. Apple compiles it with `newLibraryWithSource` and `MTLMathModeSafe` (`xcrun metal` for a
   file with a quoted `#include`, which an in-memory source cannot resolve). Apple's
   pipeline reflection gives every buffer's index, name, access and element layout, flattened
   to scalar leaves; mslc's reflection JSON has no types. The two reflections are
   cross-checked (buffer index and name); a disagreement is itself a finding.
3. Per case, every buffer is filled from one seeded generator and both backends get the same
   bytes. Cases cycle three profiles: `moderate` (small values), `wide` (random bits, finite
   floats) and `edge` (0, 1, -1, INT_MIN/MAX, UINT_MAX, +-0, +-inf, NaN, denormals, +-1e-38,
   +-1e38). bool leaves are only ever 0 or 1. `--sentinel` adds a case whose buffers are all
   0x7f bytes (a finite float), so a store outside the expected bytes shows. `--poison-padding`
   fills struct padding with 0xA5.
4. All buffers are read back and compared leaf by leaf, writable and read-only alike.
5. Negative control, every case: one leaf of mslc's result is changed and must then compare
   unequal to Apple's, so a comparison that cannot fail is detected. A run in which no leaf of
   a writable buffer was written to a non-zero value fails (`zero-output`) unless the file says
   `// DIFFCHECK: writes-zero`.

Grid and local size are in x only. `--grid` is the total thread count and must be a multiple
of `--local`. Buffers get `--elems` elements (default `max(64, 4 * grid)`). The Vulkan side sets
the local size through spec constants 0, 1 and 2, which is what indium does.

## Graphics mode

A file whose `// RUN:` line names a vertex and a fragment function (or `--vertex` and `--fragment`)
is drawn instead of dispatched:

```
// RUN: vertex=vs fragment=fs width=32 height=32 instances=2 cases=2
// DIFFCHECK-POSITION: pos          the stage_in field that carries the position, or `none`
// DIFFCHECK-GEOMETRY: 2,3 22,3 22,13 ...   triangle list, pixels, `x,y` or `x,y,z` (optional)
// DIFFCHECK: depth                 read a depth target back (implied by [[depth(any)]])
```

mslc compiles each stage separately (`--stage vertex|fragment`, spirv-val must pass). The draw is
one triangle list (default: a 20x10 and an 18x13 rectangle and a thin triangle in a 32x32 target;
no edge passes through a pixel centre) times `instances`, into colour targets chosen from the
fragment's outputs (float: 32F, int: 32I, uint: 32U, half: 16F; R, RG or RGBA to match the
component count), cleared to values no plausible output equals, and an optional D32 depth target (always-pass depth test).
The set of colour targets, their types and the depth output are read from the Metal source
(`[[color(n)]]`, `[[depth]]`, or the plain return type), never from mslc's module: an output mslc
drops, retypes or moves is reported as a `reflection-mismatch`, not left out of the comparison.
A return type the harness cannot read from the file (a typedef, a struct from an `#include`, a
3-component output, which no pixel format holds) is skipped with the reason.
Everything must stay inside the target: lavapipe returns NaN for `noperspective` varyings on a
guard-band-clipped triangle, and the backends clip differently. The Vulkan viewport has a negative
height so row 0 is the top, as in Metal.

Vertex positions must sit on the 1/256 pixel grid and the target be a power of two (so a pixel
coordinate converts to clip space exactly): the backends snap an off-grid vertex differently and
interpolated values drift apart by about 1e-3. `DIFFCHECK-GEOMETRY` is checked; a vertex shader
that moves positions (instance offsets) must keep them on the grid when the output is smooth.

Inputs: the `DIFFCHECK-POSITION` field is filled from the geometry (w = 1); every other stage_in
field is seeded per vertex, one tightly packed vertex buffer per attribute (Metal slot
`30 - location`, so a `[[buffer(n)]]` argument must not use those); `[[buffer]]` arguments are
seeded like kernel buffers, in both stages; a 2D float texture is 4x4 RGBA32F and a runtime
sampler is nearest, clamp-to-edge, normalised, no mips. Probes should sample at a texel centre.
Graphics defaults to the `moderate` profile and two cases: `wide` values interpolate to magnitudes
where any tolerance is meaningless.

Compared per attachment (named `color0`, `color1`, ... and `depth`, which `DIFFCHECK-EXCLUDE`
accepts) and per component: integers are bit-exact; a float component is bit-exact when Apple's
image gives it no more distinct values than there are primitives (plus the clear value), which
covers constants and flat varyings, and otherwise counts as interpolated and is compared within
32 ULP or 2e-5 absolute (measured worst case on the probes: 6e-6; depth within 1e-6); inexact
builtins add their `ULP_BOUNDS`. The decision reads Apple's output, so a miscompile cannot loosen
its own check. Limits: a smooth component can hide an error of up to the tolerance (a constant
added to an interpolated value, a one-ULP slip in the interpolation maths), and an interpolated
value that passes through a rounding step (`half4(in.col)` into a float target) can differ by one
half ULP between the backends.
Same vacuity guards as kernels: the negative control, the zero-output rule (a pixel must change
from the clear colour to a non-zero value on some backend; excluded attachments do not count),
a crash or a timeout of either host is an `error`, never a skip.

The Vulkan host reads back each image behind an attachment-write to transfer-read barrier and
the audited transfer-write to host-read barrier of PR #148. Vertex-stage values cannot be read
back (there is no transfer-feedback path), so a vertex miscompile shows only through what it
puts in the attachments.

Skipped with the reason (follow-up work): a writable `[[buffer]]` in either stage, vertex-stage
textures, non-2D or non-float textures, embedded samplers, framebuffer fetch, a scalar half vertex
attribute, colour outputs that are not 0..n-1 in the source. Not covered: indexed draws, blending,
stencil, multisampling, `noperspective`/`centroid` inputs, tessellation.

Graphics probes live in `tests/diffcheck/graphics` and, like the kernel probes, become ctest tests
(`diffcheck/graphics/<name>`) with the option on. `diffcheck/graphics-driver` plants eleven
miscompiles in the SPIR-V of the real output (`tests/diffcheck_mutate_mslc.py`, which takes
`MSLC_REAL`, `MUT` and `MUT_STAGE`) and requires each to be reported as a mismatch.

Textures have one mip level, so a wrong LOD or mip selection cannot be seen (tracked in #372).

## Comparison rules (`compare.py`)

| Type | Rule |
| --- | --- |
| integers, bool | bit-exact |
| float, half | bit-exact, unless the source calls an inexact builtin: then within the loosest bound of `ULP_BOUNDS` among the builtins it calls |
| NaN | two NaNs match (payloads differ legitimately); a NaN against a number never matches and never passes a ULP bound |
| infinities | the same infinity |
| +0 and -0 | different, except under a ULP bound |
| struct padding | not compared (only leaves are) |

## Exclusions, all explicit

- `// DIFFCHECK-EXCLUDE: <buffer> <reason>` skips one buffer's comparison; the reason is
  required and printed in the report. Use it for reads of uninitialised memory, out-of-bounds
  accesses, races, and the Apple quirks listed in the handover (bool-index dynamic stores write
  lanes j and j+2, vector long/ulong to bool, `half(1e-8)`, fast math).
- `// DIFFCHECK-RANGE: <buffer> <lo> <hi>` bounds the integer leaves of an index buffer.
- `// DIFFCHECK-FLOAT-RANGE: <buffer> <lo> <hi>` draws the float leaves of a buffer from the integers
  `lo..hi`, for a float the kernel converts to an index or a loop bound (so the conversion is
  defined and a `while (x) x -= 1` loop ends).
- `// RUN: grid=64 local=8 seed=1 cases=3 elems=256` sets the run parameters for the file
  (command-line flags win).
- Fused multiply-add: Metal contracts `a * b + c` into an fma even in safe math mode, mslc emits
  a separate multiply and add, and the results differ by a ULP or more under cancellation. Fusing
  inside one expression is permitted, so the Apple side compiles with `#pragma clang fp contract(off)`
  to give a deterministic reference; `--apple-contract` restores what an app's build does.
  That does not make the module safe on every driver: mslc emits no `NoContraction` and no
  `SignedZeroInfNanPreserve`, so a NIR driver may fuse, re-associate or drop -0, inf and NaN
  (#368, a real divergence, see below).
- `// DIFFCHECK-ULP: <buffer> <n>` replaces the file-wide ULP bound for one buffer (0 = bit-exact),
  so a buffer that must be exact is not loosened by a neighbour fed by `exp` (a graphics attachment
  is named `color0`, ...). It cannot loosen the interpolation tolerance of a graphics attachment.
- `// DIFFCHECK-UNUSED: <texture> <reason>` declares that the kernel ignores a texture (see below).
- Not supported yet in kernel mode, skipped with the reason: threadgroup memory arguments
  (`[[threadgroup(n)]]`: mslc rejects the attribute, #373), argument buffers, atomics (#374), writable
  textures, non-2D or non-float textures, embedded (`constexpr`) samplers (all #374). Graphics pairs have their
  own mode, below.

## Kernel textures and samplers

A kernel with `texture2d<float>` arguments (access `sample` or `read`) and runtime `sampler`
arguments is run like any other: each case seeds one 4x4 RGBA32Float texture per binding from the
case's generator (profile and seed as for buffers, so NaN and infinity texels appear in the `edge`
case) and gives both sides the same bytes. Apple binds them by `[[texture(n)]]`/`[[sampler(n)]]`,
Vulkan by the set and binding of mslc's reflection; the two reflections are cross-checked by index
and name. A sampler is nearest, clamp-to-edge, normalised, no mips (the code is shared with graphics
mode in `resources.py`), so sample at texel centres. Consequence: every runtime sampler is the same,
so a wrong sampler binding cannot change a result and is not detected.

Vacuity guard: in case 0 the texels of each texture are replaced by others and Apple's output must
change; a texture the output does not depend on is the status `unused-texture` (exit 1) unless the
file declares `// DIFFCHECK-UNUSED: <name> <reason>`. Probes are in `tests/diffcheck/kernel`
(`diffcheck/kernel/<name>`, run at grid 64, local 16) and `diffcheck/kernel-driver` plants planted
miscompiles in the SPIR-V of the real output: swapped texel coordinates, two textures swapped
between bindings, an unswapped sample coordinate. Each is reported as a mismatch while the baselines match.

## Layout cross-check

mslc's reflection JSON has no layout and a run reads only the bytes under Apple's leaf offsets on
both sides, so a layout difference in padding, or in a member the kernel never reads, was invisible.
`layout.py` reads the `Offset`, `ArrayStride` and `MatrixStride` decorations of mslc's SPIR-V and
compares the offset, size and kind (float or integer) of every scalar of one element, and the element
stride of a runtime array, with Apple's reflection. A difference, or a layout that cannot be read, is
a `reflection-mismatch`. Not covered: the total size of a `constant T &` struct (SPIR-V does not
carry it), a bool inside a buffer (reported as unreadable).

## Reading a mismatch: what is not an mslc bug

Triage of the corpus sweeps (2,754 probe kernels at 64 threads, local size 8: 1,187 compared, 1,071
matched, 71 mismatched, 45 wrote only zeros, 15 errors, the rest skipped). `tests/probe-run.txt` is
the matched and zero-output files of that sweep (one line per file, so fewer lines than kernels),
less the probes that read uninitialised memory and four that flaked across repeated runs because
every thread stores to the same element (a race, not a finding), plus the changes below. Of the 45
zero-output probes, 29 say `// DIFFCHECK: writes-zero` (zero by design; 3 of them also run `grid=1
local=1` because every thread stores to element 0); 5 were zero only because the random inputs never
reached the branch, so they bound the input with `DIFFCHECK-RANGE`; 10 race on element 0 and run
`grid=1 local=1`; 1 reads uninitialised memory and was dropped from the list.

The 71 mismatches and 15 errors (#375) are triaged. None was a silent miscompile. 62 are in the list
now, each with a `RUN`, `DIFFCHECK-RANGE` or `DIFFCHECK-FLOAT-RANGE` line. Only a `RUN` line registers a ctest
test (55 of them); the other 7 are corpus-only.

- Most (about 45) were the same race: every thread stores to element 0 or to overlapping elements, and
  the last writer differs between backends. A kernel that stores to fixed elements runs at `grid=1
  local=1`. Many of them read `thread_position_in_grid` only to pick an element, so a grid of one
  loses the thread-id coverage (no wrong value is hidden, but lane-order errors are); where the stores
  do not overlap the probe runs at the largest such grid (the `switch_repair_*` probes at 8,
  `helper_thread_pointer_*` at 3). `thread_local_array_two_dimensional_struct`
  is not a race: it reads an uninitialised element at thread 1, so it runs at `grid=1`.
  `comparison_unsigned`, already in the list, flaked in a repeat run for this reason and now runs at
  `grid=1`; the other 38 listed probes that store to a constant index while reading a thread id gave
  the same result at nine seeds.
- Inputs outside the kernel's valid range: a divisor of 0 (`DIFFCHECK-RANGE`), a bool or vector lane
  index, a buffer index, a loop bound (`for_condition_from_int`, `ternary_in_*`, `while_loop`,
  `brace_local_for*`: 13 of the 15 errors, hangs on Apple or lavapipe) and a float used as an array index
  (`local_array_*`, `FLOAT-RANGE`). `while_loop` and `while_condition_from_float` end with every
  output zero, so they say `writes-zero`; they are termination checks only (a mutant that just stores
  zero survives).
- `bool_from_float_is_unordered_compare`, `bool_vector_from_float_vector_is_unordered_compare`,
  `condition_from_float_is_unordered_compare` and `logical_and_of_numeric_operands` are not in the list:
  Apple flushes a denormal in a float compare (`x != 0.0f`) and in `bool(x)`, while mslc keeps it (#377).
  The edge profile contains denormals, and no grid size or seed keeps them out reliably (a grid that stops
  before the first denormal passes at one seed and fails at the next). Bring them back with a way to keep
  denormals out of the inputs.

Not in the list, and why:

- 12 probes that are the #368 fold, below: `brace_local_constexpr_floating`, `brace_local_mixed`,
  `brace_local_nested`, `cast_vector_converts_componentwise`, `convert_float_width`,
  `narrow_signed_kinds`, `static_cast_struct_constructor_narrows_argument`,
  `step_buffer_element_and_vector`, `half_literal_rounding_carries_into_the_exponent`,
  `construct_broadcast`, `direct_init_broadcast` and `array_member_by_value_helper` (an FMA fusion: it
  mismatches at 3 threads on seeds 2, 3 and 5). With the #368 branch built they match; add them (grid 1
  for the racing ones) when it lands.
- `pp_metal_stdlib_adds_macros`, `pp_metal_matrix_adds_fewer_macros`: Apple's in-memory compile
  pre-includes the stdlib (below).
- `struct_local_without_initialiser`, `threadgroup_array_two_dimensional`: read uninitialised memory.
- `compute_id_vector2_takes_leading_lanes`, `compute_id_ushort_vector_copies_and_passes`: every thread
  stores its id to element 0, so any grid above one races and a grid of one makes every id zero. Variants
  that index by thread match.
- `brace_elision_empty_member_explicit_braces`, `using_declaration_of_stdlib_name_hides_member_function`:
  lavapipe rejects an empty struct passed by value or built with `OpCompositeConstruct` (#376).

- Inputs that are UB for the kernel. A float input converted to an array index, a `uchar` lane
  index, a loop bound or a store index drawn from the full 32-bit range reads or writes out of
  bounds or never terminates. Use `// DIFFCHECK-RANGE`, `--range`, or `--profiles moderate`
  (many probes are single-thread designs that store to element 0: run those with `--grid 1 --local 1`,
  which hides lane-order and stride errors, or rewrite them to index by thread).
- Uninitialised reads (a struct local with no initialiser, a threadgroup array read before any
  write): Apple returns garbage, mslc zero. Exclude the buffer with a reason.
- Integer division or remainder by zero, `INT_MIN / -1`, and out-of-range float to int
  conversions are undefined: random inputs reach them, and the backends legitimately differ.
- A quoted `#include` or `__has_include("...")` cannot be resolved from an in-memory source, so
  those files take the offline `xcrun metal` route (and a relative `__has_include` path must be
  reachable from `-I`). `pp_metal_stdlib_adds_macros` matched through that route, which suggests
  `newLibraryWithSource` pre-includes `<metal_stdlib>`; that is the unverified reading of the
  other `pp_metal_*` probe.
- Lavapipe rejects some valid modules (an empty struct passed by value: `vkCreateComputePipelines` -13, #376).
- Apple flushes denormal floats in a float compare and in a float to bool conversion (#377); the
  edge profile contains denormals, so a probe that compares or converts float inputs can mismatch there.
- **Not a harness artifact, #368 (open; origin/master emits neither decoration):** lavapipe folds
  `float(half(x))` to `x`, `float(int(x))` to `trunc(x)`, `x * 0` to `0`, and re-associates
  `(x + 1) - 1`, so a half or a negative fraction held in a register, a signed zero, or a cancellation
  differs from Apple. NIR does that to every float instruction that is not `exact`; mslc's SPIR-V has no
  `NoContraction` and no `SignedZeroInfNanPreserve`, so RADV, ANV and the other NIR drivers behave the
  same. These mismatches are mslc results, not harness noise.

Whether to compare through a buffer store by default: not yet. A store forces the rounding only for the
value that is stored, so it hides exactly the register-held cases above (the reason to look at them),
and it does not help the other two limits (the `<metal_stdlib>` pre-include and uninitialised reads
are Apple-side, not folded values). Until #368 lands, keep comparing as written: where a probe's
mismatch is the #368 fold, exclude the buffer with a reason that names #368. Re-measured with the
#368 branch (not on master yet): the 12 fold probes listed above all match, so nothing more is needed
and no buffer-store mode should be added. Repeat it once #368 is on master.

## Tests and CI

`-DMSLC_DIFFCHECK=ON` registers `diffcheck/*` tests; the default build and `ctest` are
unchanged. The pure-Python tests (`compare`, `inputs`, `checks`) run anywhere; the tests that
need a Metal device or lavapipe exit 77, which ctest reports as skipped. The Apple half needs
a Metal device, and lavapipe needs Mesa installed, so this is a local and opt-in check.
`.github/workflows/diffcheck.yml` is an advisory macOS job (`continue-on-error`): its first step,
`tools/diffcheck/ci-probe-metal.sh`, logs whether the runner has a Metal device and skips the rest
when it does not. Unproven on a real runner: read that step's log on the first run (a device named
under `devices:` and `default: present` means the job runs; `devices: 0` means a self-hosted runner is
the only way). A failing job-level `continue-on-error` job may still show a red check on the commit;
that is unverified until the first run. `tests/probe-run.txt` lists the kernel probes that match (at the default grid or the one their `RUN` line names); the job
requires 1,105 compared (the list compares 1,173 kernels).

A probe in `tests/probes/` opts in with a `// RUN:` line; with the option on it becomes the
ctest test `diffcheck/probe/<name>` and kills a mutant the disassembly regexes cannot.
