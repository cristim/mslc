// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %v2bool
// DISASM-NO-MATCH: = OpFOrdNotEqual
//
// != on float vectors is unordered, so NaN != x is true in that lane.
kernel void comparison_float_vector_not_equal_unordered(device int2 *out [[buffer(0)]],
    constant float2 *a [[buffer(1)]], constant float2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] != b[i]);
}
