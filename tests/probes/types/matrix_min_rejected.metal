// EXPECT: error min takes scalars or vectors, not a matrix
// GLSL.std.450 FMin takes no matrix, and Apple has no min for one either.
kernel void matrix_min_rejected(device float4 *out [[buffer(0)]],
                                device const float4x4 *m [[buffer(1)]],
                                uint i [[thread_position_in_grid]])
{
    float4x4 smaller = min(m[0], m[1]);
    out[i] = smaller[0];
}
