// EXPECT: error cannot store through "p"
//
// Apple: read-only variable is not assignable.
kernel void store_vector_element_to_constant_buffer_rejected(device float *out [[buffer(0)]],
                                                   constant float4 *p [[buffer(1)]],
                                                   uint i [[thread_position_in_grid]])
{
    p[0][i & 3] = 1.0;
    out[0] = p[0].x;
}
