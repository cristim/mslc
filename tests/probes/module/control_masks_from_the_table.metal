// EXPECT: valid
// The three control masks the emitter writes. Each is None, and each was a bare
// 0 because the generated table carried no mask categories, so the value could
// not be checked against the grammar. These are patterns rather than
// substrings because a bare "None" is satisfied by any one of the three, and a
// substring cannot say which instruction a trailing operand belongs to.
// DISASM-MATCH: OpFunction %void None
// DISASM-MATCH: OpSelectionMerge %[0-9]+ None
// DISASM-MATCH: OpLoopMerge %[0-9]+ %[0-9]+ None
// DISASM-NO-MATCH: OpFunction %void Inline
// DISASM-NO-MATCH: OpFunction %void DontInline
// DISASM-NO-MATCH: OpFunction %void Pure
// DISASM-NO-MATCH: OpFunction %void Const
// DISASM-NO-MATCH: OpSelectionMerge %[0-9]+ Flatten
// DISASM-NO-MATCH: OpSelectionMerge %[0-9]+ DontFlatten
// DISASM-NO-MATCH: OpLoopMerge %[0-9]+ %[0-9]+ Unroll
// DISASM-NO-MATCH: OpLoopMerge %[0-9]+ %[0-9]+ DontUnroll
kernel void control_masks_from_the_table(device uint* out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{
    if (i < 4u) { out[i] = 1u; } else { out[i] = 2u; }
    for (uint j = 0u; j < 2u; j = j + 1u) { out[i] = out[i] + j; }
}
