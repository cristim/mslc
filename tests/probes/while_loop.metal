// EXPECT: valid
// DISASM: OpLoopMerge
kernel void while_loop(device uint* out [[buffer(0)]],
                       uint i [[thread_position_in_grid]])
{
    while (out[i] > 0u) { out[i] = out[i] - 1u; }
}
