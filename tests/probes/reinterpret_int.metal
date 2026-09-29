// EXPECT: valid
// A change of signedness at the same width is a reinterpretation, so it stays
// OpBitcast. OpUConvert and OpSConvert need a different width, and spirv-val
// rejects them at equal width.
// DISASM: OpBitcast
kernel void reinterpret_int(device const uint* w [[buffer(0)]],
                            device int* out [[buffer(1)]],
                            uint i [[thread_position_in_grid]])
{ out[i] = w[i]; }
