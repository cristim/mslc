# Known issues

## lavapipe cannot execute any mslc module on this machine

**Status:** environment, not mslc. Found 2026-10-10 while verifying #181.

Running any mslc-produced compute module on lavapipe (mesa 26.2.4, LLVM 23.1.2,
`libvulkan_lvp.dylib`) fails at `vkCreateComputePipelines` with
`VK_ERROR_FEATURE_NOT_PRESENT` (-13), and a dispatch that does get that far
segfaults inside the driver.

Evidence that this is the driver and not mslc or the harness:

- The device is found, reports Vulkan API 1.4.354, exposes `dynamicRendering`,
  `maintenance4` and `VK_KHR_synchronization2`, and creates instance, device,
  queue, buffer, memory and descriptor set without complaint.
- `spirv-val --target-env vulkan1.3` accepts the same modules.
- It reproduces with `tests/compute_buffer_clear.metal` and with a four-line
  kernel that only stores a literal, so it is not a specific feature mslc emits.
- `lldb` puts the fault in `libvulkan_lvp.dylib`'s worker thread
  (`___lldb_unnamed_symbol14182 + 356`), with no mslc or harness frame below it.
- It is unaffected by `spirv-opt --freeze-spec-const`, so
  `OpExecutionModeId ... LocalSizeId` spec constants are not the cause.

Consequence: the read-back evidence AGENTS.md asks for cannot be produced on
this host. Work here is verified by `spirv-val` validity, disassembly, and
differential comparison against Apple's compiler; produced-value checks have to
move to a host where lavapipe works, or to an Apple GPU.

**Do not** treat a failed lavapipe run as a finding about mslc until this is
retested against a working driver.

## mslc emits Vulkan-valid modules that this lavapipe rejects

`PhysicalStorageBuffer64` addressing with a binding-0 address block is correct
for indium and is validated by `spirv-val`, but a driver may not implement it.
Where a read-back is required for such a module, confirm the driver supports
the addressing model before treating a failure as an mslc defect.
