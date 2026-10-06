// EXPECT: valid
// DISASM: = OpConstant %ulong 4294967296
//
// A u literal is a uint while it fits and a ulong after.
kernel void literal_unsigned_above_uint_max_is_ulong(device ulong* out [[buffer(0)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = 4294967296u;
}
