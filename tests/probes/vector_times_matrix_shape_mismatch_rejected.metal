// EXPECT: error the operands of a matrix product do not match
// A row vector multiplies a matrix with as many rows as it has components, and
// a float2x2 has two rows, not three. Apple rejects the product too.
kernel void vector_times_matrix_shape_mismatch_rejected(device float2 *out [[buffer(0)]],
                                                        device const float2x2 *m [[buffer(1)]],
                                                        device const float3 *in [[buffer(2)]],
                                                        uint i [[thread_position_in_grid]])
{
    out[i] = in[i] * m[0];
}
