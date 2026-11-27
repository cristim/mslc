// EXPECT: valid
// An 8-bit integer type needs Int8.
// DISASM: OpCapability Int8
kernel void char_scalar(device const uchar* in [[buffer(0)]],
                        device uchar* out [[buffer(1)]],
                        uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
