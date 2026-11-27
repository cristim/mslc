// EXPECT: valid
// A 16-bit or 8-bit integer type needs the Int16 or Int8 capability, which the
// scalar table never emitted. Long and ulong already asked for Int64.
// DISASM: OpCapability Int16
kernel void short_scalar(device const ushort* in [[buffer(0)]],
                         device ushort* out [[buffer(1)]],
                         uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
