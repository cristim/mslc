// EXPECT: valid
// MSL's % truncates like C, so a signed remainder takes the dividend's sign
// (OpSRem); OpSMod takes the divisor's.
// DISASM: OpSDiv
// DISASM: OpSRem
// DISASM-NOT: OpSMod
kernel void int_div_rem(device const int* in [[buffer(0)]],
                        device int* out [[buffer(1)]],
                        uint i [[thread_position_in_grid]])
{ out[i] = in[i] / 3 + in[i] % 5; }
