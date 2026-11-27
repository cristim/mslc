// EXPECT: error the operands of an operator are vectors of different widths
//
kernel void comparison_vector_widths_differ_rejected(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int3 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] == b[i]);
}
