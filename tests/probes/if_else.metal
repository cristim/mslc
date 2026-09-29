// EXPECT: valid
// DISASM: OpSelectionMerge
kernel void if_else(device uint* out [[buffer(0)]],
                    uint i [[thread_position_in_grid]])
{
    if (i < 4u) { out[i] = 1u; } else { out[i] = 2u; }
}
