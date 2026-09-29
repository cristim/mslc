// EXPECT: valid
// OpConstant's literal count follows the width, so a 64-bit zero needs two
// words. One word made a short instruction.
// DISASM: OpConstant %ulong
kernel void uninitialised_wide_local(device const ulong* in [[buffer(0)]],
                                     device ulong* out [[buffer(1)]],
                                     uint i [[thread_position_in_grid]])
{
    ulong acc;
    if (in[i] > acc) { acc = in[i]; }
    out[i] = acc;
}
