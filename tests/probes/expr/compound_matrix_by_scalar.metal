// EXPECT: valid
// DISASM-MATCH: = OpMatrixTimesScalar %mat4v4float %[0-9]+ %float_2
//
// float4x4 m; m *= 2.0f scales the matrix in place.
kernel void compound_matrix_by_scalar(device float4x4 *out [[buffer(0)]],
                                      uint i [[thread_position_in_grid]])
{
    float4x4 x = out[i];
    x *= 2.0f;
    out[i] = x;
}
