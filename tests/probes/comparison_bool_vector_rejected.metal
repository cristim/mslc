// EXPECT: error a bool operand is not lowered yet
//
// Apple accepts bool2 == bool2; mslc does not lower a bool operand yet.
kernel void comparison_bool_vector_rejected(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    bool2 x = a[i] == b[i];
    out[i] = int2(x == x);
}
