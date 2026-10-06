// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %v2bool
// DISASM-NO-MATCH: = OpFOrdNotEqual
//
// The same for half vectors.
kernel void comparison_half_vector_not_equal_unordered(device int2 *out [[buffer(0)]],
    constant half2 *a [[buffer(1)]], constant half2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] != b[i]);
}
