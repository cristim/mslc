// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float %int_2
// DISASM-MATCH: = OpMatrixTimesScalar %mat4v4float
//
// float4x4 m; m *= 2 converts the scalar to the matrix's component type, as Apple's compiler does
// for a compound assignment.
kernel void compound_matrix_by_int_scalar_converts(device float4x4 *out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    float4x4 x = out[i];
    x *= 2;
    out[i] = x;
}
