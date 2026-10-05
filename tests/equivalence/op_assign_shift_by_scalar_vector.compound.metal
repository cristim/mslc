// A vector shifted by a scalar: the plain shift spreads the count the same way.
kernel void shift_by_scalar_vector(device uint4 *out [[buffer(0)]], device const uint4 *in [[buffer(1)]],
                                   uint i [[thread_position_in_grid]])
{
    uint4 x = in[i];
    x <<= 3u;
    x >>= 1u;
    out[i] = x;
}
