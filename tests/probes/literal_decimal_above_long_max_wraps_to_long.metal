// EXPECT: valid
// DISASM: = OpConstant %long -9223372036854775808
// DISASM-NOT: = OpConstant %ulong
//
// Apple's compiler types a decimal literal past the largest long as a long and
// keeps its low 64 bits, where C would make it unsigned. Compiled on the GPU,
// 9223372036854775808 / 2 is -4611686018427387904, so the wrap is followed rather
// than read as the unsigned the standard would give.
kernel void literal_decimal_above_long_max_wraps_to_long(device long* out [[buffer(0)]],
                                                         uint i [[thread_position_in_grid]])
{
    out[i] = 9223372036854775808;
}
