// EXPECT: valid
// DISASM: = OpConstant %half 0x1p+16
// DISASM-NOT: = OpConstant %half -0x1p+16

//
// 2^63 as a ulong is positive, and is infinity as a half.
constant half kValue = 9223372036854775808ul;

kernel void file_scope_constant_ulong_converts_to_half_unsigned(device half* out [[buffer(0)]],
                                                                uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
