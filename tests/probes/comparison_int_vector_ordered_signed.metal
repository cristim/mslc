// EXPECT: valid
// DISASM-MATCH: = OpSLessThan %v2bool
// DISASM-MATCH: = OpSLessThanEqual %v2bool
// DISASM-MATCH: = OpSGreaterThan %v2bool
// DISASM-MATCH: = OpSGreaterThanEqual %v2bool
//
// The four orderings on int vectors are signed and give a bool vector.
kernel void comparison_int_vector_ordered_signed(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] < b[i]) + int2(a[i] <= b[i]) + int2(a[i] > b[i]) + int2(a[i] >= b[i]);
}
