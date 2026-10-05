// EXPECT: error on a vector takes a scalar or a vector of the same type
//
// Apple accepts only a vector of the same type, or a scalar.
kernel void compound_vector_by_other_vector_rejected(device int4 *out [[buffer(0)]], device const uint4 *u [[buffer(1)]],
                                                     uint i [[thread_position_in_grid]])
{
    int4 x = out[i];
    x += u[i];
    out[i] = x;
}
