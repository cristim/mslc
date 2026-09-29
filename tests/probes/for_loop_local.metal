// EXPECT: valid
// DISASM: OpLoopMerge
kernel void for_loop_local(device const float* in [[buffer(0)]],
                           device float* out [[buffer(1)]],
                           uint i [[thread_position_in_grid]])
{
    float s = 0.0;
    for (uint j = 0u; j < 4u; j = j + 1u) { s = s + in[i * 4u + j]; }
    out[i] = s;
}
