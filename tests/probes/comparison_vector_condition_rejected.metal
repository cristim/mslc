// EXPECT: error a condition has to be a bool or a numeric scalar
//
// Apple rejects a vector condition, and a comparison of two vectors is one.
kernel void comparison_vector_condition_rejected(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    if (a[i] == b[i]) { out[i] = int2(1); }
}
