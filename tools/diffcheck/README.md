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

A corpus list has one path per line, `#` comments, an optional `std=3.1` and the fields of a
`// RUN:` line (`vertex=vs fragment=fs width=32 ...`), which override the file's own, so a file you do
not edit can be drawn as a graphics pair. An unknown or malformed field, an empty `std=`, a repeated `std=`, or a field in the path position is reported
as an error, naming it, so the run exits with an error (even if `--min-compared` is also unmet), and the other lines still run. A path containing `=` is not supported: the line is split on whitespace with no escaping, so it reads as a field. Paths are
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
   floats) and `edge` (0, 1, -1, INT_MIN/MAX, UINT_MAX, +-0, +-inf, NaN, +-1.2e-38, +-1e38; float32 denormals are never generated, because Apple flushes them in a compare and mslc does not, #377: `denormal_compare_flush.metal` pins that difference). bool leaves are only ever 0 or 1. `--sentinel` adds a case whose buffers are all
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
seeded like kernel buffers, in both stages; a 2D texture is 4x4 RGBA of `float`, `half`,
`int`, `uint`, `short` or `ushort` (`sample` or `access::read`), a runtime sampler is nearest,
normalised, no mips with address mode picked by its Metal index (`0` clamp-to-edge, `1` repeat,
`2` mirrored repeat), and an embedded (`constexpr`) sampler is built on Vulkan from mslc's
reflection. Probes should sample at a texel centre (or 1/1024-grid coordinates for linear filtering).
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

Skipped with the reason (follow-up work): a writable `[[buffer]]` or writable texture (`access::write`
or `access::read_write`) in either stage, non-2D textures, framebuffer fetch, a scalar half vertex
attribute, colour outputs that are not 0..n-1 in the source. Not covered: indexed draws, blending,
stencil, multisampling, `noperspective`/`centroid` inputs, tessellation.

Graphics probes live in `tests/diffcheck/graphics` and, like the kernel probes, become ctest tests
(`diffcheck/graphics/<name>`) with the option on. `diffcheck/graphics-driver` plants 22
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
  mslc decorates every float conversion, `OpFAdd`, `OpFSub` and `OpFMul` `NoContraction` and sets
  `SignedZeroInfNanPreserve` (32, and 16 when half is used) on each entry point (#368), so a NIR
  driver no longer folds, re-associates or drops -0, inf and NaN, and it does not fuse a separate
  multiply and add either. Apple's in-expression fusion stays out of scope (known-issues.md).
- `// DIFFCHECK-ULP: <buffer> <n>` replaces the file-wide ULP bound for one buffer (0 = bit-exact),
  so a buffer that must be exact is not loosened by a neighbour fed by `exp` (a graphics attachment
  is named `color0`, ...). In a simple single-entry kernel without control flow or aliasing,
  `ulp_bounds_by_root` derives each buffer's (and each scalar struct field's) ULP bound from the
  expressions stored into it automatically. It cannot loosen the interpolation tolerance of a
  graphics attachment.
- `// DIFFCHECK-TEXEL: <texture_buffer> <1|2|4>` runs that texel buffer with R, RG or RGBA texels (default 4; see Kernel texel buffers).
- `// DIFFCHECK-UNUSED: <texture> <reason>` declares that the kernel ignores a texture (see below).
- Not supported yet in kernel mode, skipped with the reason: argument buffers, atomics (#374),
  non-2D textures (#374). Graphics pairs have their
  own mode, below.

## Kernel threadgroup memory arguments

`threadgroup T *p [[threadgroup(n)]]` gets room for one element per thread of the group: Apple's
`setThreadgroupMemoryLength:atIndex:` is called with `local * element_size` bytes rounded up to a multiple of 16 (Apple's requirement), and the Vulkan host
writes `local` into the argument's specialization constant (`spec_id` of mslc's `ThreadgroupMemory`
reflection entry, `spec <id> <value>` in vkhost's manifest). The indices, names and element sizes of
mslc's reflection are cross-checked against Apple's threadgroup bindings. A kernel must therefore index
its threadgroup arguments below the local size.

## Kernel textures and samplers

A kernel with `texture2d<T>` arguments (T: float, half, int, uint, short or ushort; access `sample`, `read`, `write`
or `read_write`) and runtime `sampler` arguments is run like any other: each case seeds one 4x4 RGBA texture of T
(RGBA32Float, RGBA16Float, RGBA32Sint, RGBA32Uint, RGBA16Sint or RGBA16Uint) per binding from the case's generator
(profile and seed as for buffers, so NaN and infinity texels appear in the `edge` case) and gives both sides the same
bytes. Apple binds them by `[[texture(n)]]`/`[[sampler(n)]]`,
Vulkan by the set and binding of mslc's reflection; the two reflections are cross-checked by index,
name and access, and the element type comes from Apple's reflection. After the dispatch, every `write` or
`read_write` texture's 4x4 texels are read back and compared leaf by leaf (4 leaves of T per texel, so texels no
thread writes must keep their seed), and the negative control perturbs one writable texture as its own group. A sampler is nearest, normalised, no mips, and its address mode follows its Metal index: 0 clamp to
edge, 1 repeat, 2 mirrored repeat (repeating, so index 3 is clamp again). Sample at texel centres.
Samplers with different indices therefore read different texels outside [0, 1], and a swapped sampler
binding is visible, but only between indices that differ modulo 3 and only for coordinates outside the
unit square; the filter is always nearest, so a wrong filter state is not detected (#374).

An embedded (`constexpr`) sampler in a kernel is in Apple's shader, so Apple gets no binding for it; the Vulkan
host builds it from mslc's `embedded_samplers` state (address modes, nearest or linear). A state it cannot build (mips,
compare, unnormalised coordinates, anisotropy, a min filter that differs from the mag filter, a lod range) is skipped
with the reason. Known gap: a linear filter on a format without the linear-filter feature bit is not checked.

Vacuity guard: in case 0 the texels of each `sample`, `read` or `read_write` texture are replaced by others and Apple's output must
change; a texture the output does not depend on is the status `unused-texture` (exit 1) unless the
file declares `// DIFFCHECK-UNUSED: <name> <reason>`. For a `read_write` texture its own texels count only at
coordinates the kernel wrote, so a texture the kernel merely overwrites without reading is unused; a write-only
texture is an output and is not tested this way. A declaration for a texture that is in fact used
is an `error`, so a stale directive cannot hide a texture. Probes are in `tests/diffcheck/kernel`
(`diffcheck/kernel/<name>`, run at grid 64 or 16, local 16) and `diffcheck/kernel-driver` plants
miscompiles in the SPIR-V of the real output: swapped texel or write coordinates, swapped written float4 or int4
components, two textures (sampled, read-only, write-only or read-write) swapped between bindings, an unswapped sample coordinate,
two samplers swapped between bindings. Each is reported as a mismatch while the baselines match.

## Kernel texel buffers

A kernel with `texture_buffer<T, access>` arguments (T: float, half, int, uint, short or ushort; access read,
write or read_write) is run too, `get_width` included. Each is a buffer of texels of T (RGBA unless `// DIFFCHECK-TEXEL: <name> 1|2`
picks R or RG; `elems` texels, so 256 at
grid 64), seeded from the case's generator like a device buffer (profiles, `DIFFCHECK-RANGE` and `-FLOAT-RANGE` by name) and given
identical bytes on both sides. Apple binds a `MTLTextureTypeTextureBuffer` over a shared buffer at the Metal index
(pixel format RGBA32Float, RGBA16Float, RGBA32Sint, RGBA32Uint, RGBA16Sint, RGBA16Uint, or the R and RG ones of the same
types); Vulkan binds a `VkBufferView` of the matching `R32G32B32A32_*`, `R32G32_*`, `R32_*` or `R16...` format at the descriptor mslc's reflection names
(`UniformTexelBuffer` for access::read, `StorageTexelBuffer` otherwise). The element type and access come from
Apple's reflection; mslc's reflection must agree on index, name, access and descriptor type or the file is a
`reflection-mismatch`. After the dispatch the bytes of every texel buffer are read back and compared leaf by leaf
(one per component, four for RGBA) like a device buffer, so a write-only texel buffer is an output and the texels no thread writes
must keep their seed. The negative control perturbs one writable device buffer and one writable texel buffer
separately; both must compare unequal.

Vacuity guard, as for textures: in case 0 the contents of each read or read_write texel buffer are replaced and
Apple's output must change, else `unused-texture` (declare `// DIFFCHECK-UNUSED: <name> <reason>`). For a read_write
buffer its own contents count only at texels the kernel wrote, so a buffer the kernel merely overwrites is unused.
A write-only buffer is an output and is not tested this way.

A host that cannot make the buffer says `unsupported: <reason>` and the file is skipped with it, never passed:
a Vulkan format without the texel buffer feature, a device without `shaderStorageImageRead/WriteWithoutFormat` (a
storage texel buffer needs both), more texels than `maxTexelBufferElements`, or no Metal texture over the buffer.
Lavapipe and Apple Silicon do all six types in both access modes and with 1, 2 and 4 components. A texel of fewer than four
components reads as `(r, 0, 0, 1)` or `(r, g, 0, 1)` and a write keeps only the components the format has, on both sides; mslc's
SPIR-V carries no format (OpTypeImage `Unknown`), so these probes pin that it neither invents the missing components nor
depends on a count it cannot see. A swap of a read and a read_write binding is a descriptor type mismatch; lavapipe
currently crashes on it, which is reported as an `error`, not a mismatch. The texel count is `elems` plus 4 per Metal
index plus the case number, so every buffer and every case has a different `get_width`; a kernel must index buffers
only below the width of the lowest-numbered one.
Probes (`tests/diffcheck/kernel/texel_buffer_*`; the `_r_`, `_rg_` and `narrow` ones use `DIFFCHECK-TEXEL`) are run at grid 64, local 16, and
`diffcheck/kernel-driver` plants nine miscompiles (a texel index off by one, a mirrored index off by one, every write two texels away, two components
swapped, three binding swaps, `get_width` folded to a constant, `get_width` of the wrong buffer) and requires each to be reported as a mismatch while the baselines match. Five more are specific to R and RG buffers (a read that
repeats x or y in a later component, a write with x and y or y and z swapped, an R and an RG buffer exchanging bindings), and
swapping z and w of a write to an RG buffer, which changes no stored byte, must still match.

## Layout cross-check

mslc's reflection JSON has no layout and a run reads only the bytes under Apple's leaf offsets on
both sides, so a layout difference in padding, or in a member the kernel never reads, was invisible.
`layout.py` reads the `Offset`, `ArrayStride` and `MatrixStride` decorations of mslc's SPIR-V and
compares the offset, size and kind (float or integer) of every scalar of one element, in member
order (so two members that swap offsets differ), and the element stride of a runtime array, with
Apple's reflection. Matrices must be `ColMajor`: a `RowMajor` one, which has the same offsets when
square, is a difference. A difference, or a layout that cannot be read, is a `reflection-mismatch`.
Not covered: the total size of a `constant T &` struct (SPIR-V does not carry it). A bool in a buffer
is stored by mslc as a byte, and read and compared like a uchar.

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
  `condition_from_float_is_unordered_compare` and `logical_and_of_numeric_operands` are back in the list:
  Apple flushes a denormal in a float compare (`x != 0.0f`) and in `bool(x)`, while mslc keeps it (#377).
  The inputs no longer contain float32 denormals (`no_denormal_float`), so all four match across moderate,
  wide and edge profiles.

Not in the list, and why:

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
- Apple flushes denormal floats in a float compare and in a float to bool conversion; mslc declares no
  DenormFlushToZero (#377). The inputs contain no float32 denormal; `tests/diffcheck/denormal_compare_flush.metal`
  pins the difference (Apple 0, mslc 1), checked by `tests/diffcheck_denormal_test.py`.
- **Fixed, #368:** lavapipe used to fold `float(half(x))` to `x`, `float(int(x))` to `trunc(x)`,
  `x * 0` to `0` and `(x + 1) - 1` to `x`, because mslc's SPIR-V carried no `NoContraction` and no
  `SignedZeroInfNanPreserve`. The `safe_math_*` probes pin each case; a mismatch of that shape now is a new bug.

A buffer-store comparison mode is not needed: re-measure the corpus on this fix and add one only if fold mismatches remain.

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
requires at least 1,105 compared.

A probe in `tests/probes/` opts in with a `// RUN:` line; with the option on it becomes the
ctest test `diffcheck/probe/<name>` and kills a mutant the disassembly regexes cannot.
