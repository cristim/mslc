// EXPECT: valid
// DISASM: OpMatrixTimesVector %v4float
//
// A matrix on the left of a vector is the transform the vertex shaders in
// indium's cube and lighting tests apply.
kernel void matrix_times_vector(device float4 *out [[buffer(0)]],
                                device const float4x4 *m [[buffer(1)]],
                                device const float4 *in [[buffer(2)]],
                                uint i [[thread_position_in_grid]])
{
    out[i] = m[0] * in[i];
}
