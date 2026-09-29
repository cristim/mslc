// EXPECT: valid
// DISASM: OpUDiv
// DISASM: OpUMod
// DISASM-NOT: OpSDiv
// DISASM-NOT: OpSMod
kernel void uint_div_mod(device const uint* in [[buffer(0)]],
                         device uint* out [[buffer(1)]],
                         uint i [[thread_position_in_grid]])
{ out[i] = in[i] / 3u + in[i] % 5u; }
