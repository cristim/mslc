// EXPECT: valid
// DISASM: = OpConstant %ulong 18446744073709551615
//
// The largest ulong, which only fits in 64 bits as an unsigned.
constant ulong kValue = 0xffffffffffffffff;

kernel void file_scope_constant_ulong_holds_every_bit(device ulong* out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
