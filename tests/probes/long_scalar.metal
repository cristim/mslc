// EXPECT: valid
// Int64 for a 64-bit integer, which the table already asked for but emitted
// into the wrong section.
// DISASM: OpCapability Int64
kernel void long_scalar(device const ulong* in [[buffer(0)]],
                        device ulong* out [[buffer(1)]],
                        uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
