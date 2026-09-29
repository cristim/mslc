// EXPECT: valid
// The unsigned side, so the fix above cannot be a blanket OpConvertSToF.
// DISASM: OpConvertUToF
// DISASM-NOT: OpConvertSToF
kernel void uint_to_float(device const uint* in [[buffer(0)]],
                          device float* out [[buffer(1)]],
                          uint i [[thread_position_in_grid]])
{ out[i] = float(in[i]); }
