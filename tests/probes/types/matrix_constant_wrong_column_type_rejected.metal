// EXPECT: error a column of this matrix has to be a float2 already
// Apple rejects a float3 column for a float2x2 too.
constant float2x2 m = float2x2(float3(1), float3(2));

kernel void matrix_constant_wrong_column_type_rejected(device float *out [[buffer(0)]])
{
    out[0] = m[0][0];
}
