# Known issues

## lavapipe cannot execute any mslc module on this machine (RESOLVED, stale)

**Status:** resolved/stale as of 2026-10-13; kept for history, not current guidance.
Originally found 2026-10-10 while verifying #181.

This entry claimed every mslc-produced compute module failed at
`vkCreateComputePipelines` with `VK_ERROR_FEATURE_NOT_PRESENT`, or segfaulted
inside the driver. That is no longer true on this machine: fragment and
compute/kernel lavapipe readbacks have since run cleanly and produced correct,
mutation-sensitive numeric results for #184 (point_size), #220 (texture3d),
#221 (texture2d_array), and #173 (half fwidth), across several sessions
between 2026-10-10 and 2026-10-13. Whatever the original environment fault
was (driver update, ICD reinstall, or a transient local misconfiguration on
this shared machine, never isolated), it no longer reproduces. Left here
rather than deleted so a future reader who hits a *new* lavapipe failure has
the original evidence and knows this specific failure mode was already seen
and later resolved, not to be treated as still-current advice to skip
readbacks on this host.

Original evidence (2026-10-10):

- The device was found, reported Vulkan API 1.4.354, exposed `dynamicRendering`,
  `maintenance4` and `VK_KHR_synchronization2`, and created instance, device,
  queue, buffer, memory and descriptor set without complaint.
- `spirv-val --target-env vulkan1.3` accepted the same modules.
- It reproduced with `tests/compute_buffer_clear.metal` and with a four-line
  kernel that only stores a literal, so it was not a specific feature mslc
  emitted.
- `lldb` put the fault in `libvulkan_lvp.dylib`'s worker thread
  (`___lldb_unnamed_symbol14182 + 356`), with no mslc or harness frame below it.
- It was unaffected by `spirv-opt --freeze-spec-const`, so
  `OpExecutionModeId ... LocalSizeId` spec constants were not the cause.

**Do** attempt a lavapipe readback on this machine; it works as of 2026-10-13.
If a *new* failure reproduces, record fresh evidence rather than reviving this
entry, since the original cause was never identified.

## half `fwidth` lowers via widen-to-float32, Apple parity unmeasured (#173)

`fwidth` on a `half`/`half2`/`half3`/`half4` argument lowers by converting the
operand to `float32`, emitting `OpFwidth` (core SPIR-V requires 32-bit float
components), and converting the result back to `half`. This is proven
internally correct by a lavapipe readback (exact affine-input derivative,
tolerance 0, two compiler mutants both caught), but **Apple's own half-fwidth
rounding policy is unmeasured on this machine** (no Apple toolchain is
reachable here). Apple is known to emit a native `air.fwidth.v2f16` for half,
which may compute the difference/sum in half precision directly rather than
widening to float32 first; the two approaches are not guaranteed to produce
bit-identical results. The project owner was asked directly and chose to land
this lowering with the gap stated rather than wait for hardware access to
measure it. If Apple hardware/toolchain access becomes available, measure
`air.fwidth.v2f16`'s actual output against mslc's widen-compute-narrow output
and update or remove this entry.

## mslc emits Vulkan-valid modules that this lavapipe rejects

`PhysicalStorageBuffer64` addressing with a binding-0 address block is correct
for indium and is validated by `spirv-val`, but a driver may not implement it.
Where a read-back is required for such a module, confirm the driver supports
the addressing model before treating a failure as an mslc defect.
