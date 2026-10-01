// EXPECT: valid
// DISASM: OpVariable %_ptr_Function_uint Function
// DISASM-ORDER: %50 = OpFunction %void None
// DISASM-ORDER: %62 = OpVariable %_ptr_Function_uint Function
//
// Locals belong to their own function's first block, and the module has as many
// functions as it has entry points. Every Function-storage OpVariable used to go
// into one module-wide bucket that finalize() spliced after the **first** OpLabel
// of the Functions section, justified by the comment "the module has one
// function". With two entry points, entry point 2's locals were declared in entry
// point 1's entry block, and the module was a dominance error:
//
//   mslc: wrote out.spv (1784 bytes)          <- reported as a success
//   error: line 79: ID '79[%79]' defined in block '36[%36]' does not dominate
//          its use in block '69[%69]'
//
// Reachable through mslc_translate, so through newLibraryWithSource:. The
// existing two_entry_points_per_module probe did not catch it because neither of
// its functions declares a local.
//
// Two vertex functions rather than two kernels: a module has one set of three
// workgroup-size spec constants and indium specialises them once per pipeline, so
// mslc refuses a second kernel. That refusal happens before any body is lowered,
// which is what makes this the only way to reach a second function here.
//
// The order is the claim, and the pair is one assertion: the first function
// declares no local at all, so every OpVariable in the module belongs to the
// second, and it is emitted after the second OpFunction. A splice that ignored
// the function id would put both in the first function's block, which is before
// the second OpFunction, and the pair would then read the other way round.
//
// The ids are pinned rather than matched, because CMake's REGEX MATCH does not
// span a newline, so no pattern can reach across the two functions to say which
// block a line is in. DISASM-ORDER compares positions instead, and the two ids
// are stable because the first function's body is fixed by the probe.
vertex void vertex_without_locals(device uint *in [[buffer(0)]],
                                  device uint *out [[buffer(1)]],
                                  uint index [[vertex_id]])
{
    out[index] = in[index];
}

vertex void vertex_with_a_local_and_a_loop(device uint *in [[buffer(0)]],
                                          device uint *out [[buffer(1)]],
                                          uint index [[vertex_id]])
{
    uint total = 0u;
    for (uint step = 0u; step < 4u; step = step + 1u) {
        total = total + in[step];
    }
    out[index] = total;
}
