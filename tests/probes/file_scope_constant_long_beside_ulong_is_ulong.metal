// EXPECT: valid
// DISASM: = OpConstant %ulong 18446744073709551615

//
// A long beside a ulong is a ulong, so -1 is converted to the largest ulong and
// divided unsigned.
constant long kA = -1;
constant ulong kValue = kA / 1ul;

kernel void file_scope_constant_long_beside_ulong_is_ulong(device ulong* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
