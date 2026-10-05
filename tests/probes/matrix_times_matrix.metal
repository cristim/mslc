// EXPECT: valid
// DISASM: OpMatrixTimesMatrix %mat2v4float
//
// A float3x4 (three columns of four rows) times a float2x3 (two columns of
// three) is a float2x4: the right's column count and the left's row count. A
// square pair could not tell the two apart.
kernel void matrix_times_matrix(device float4 *out [[buffer(0)]],
                                device const float3x4 *left [[buffer(1)]],
                                device const float2x3 *right [[buffer(2)]],
                                device const float2 *in [[buffer(3)]],
                                uint i [[thread_position_in_grid]])
{
    float2x4 product = left[0] * right[0];
    out[i] = product * in[i];
}
