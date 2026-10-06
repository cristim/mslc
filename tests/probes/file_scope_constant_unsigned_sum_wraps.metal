// EXPECT: valid
// DISASM: = OpConstant %uint 0
//
// Unsigned arithmetic wraps by definition: 4294967295u + 1 is 0.
constant uint kValue = 4294967295u + 1;

kernel void file_scope_constant_unsigned_sum_wraps(device uint* out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
