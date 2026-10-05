// EXPECT: error is a float4x4 reached by reference, which is not lowered yet
// A matrix's MatrixStride can only be stated on a struct member, and a buffer
// reached by reference is not wrapped in one.
kernel void matrix_reference_parameter_rejected(device float4 *out [[buffer(0)]],
                                                constant float4x4 &m [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = m * out[i];
}
