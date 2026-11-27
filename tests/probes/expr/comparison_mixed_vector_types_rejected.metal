// EXPECT: error an operator takes two vectors of the same type
//
kernel void comparison_mixed_vector_types_rejected(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant uint2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] == b[i]);
}
