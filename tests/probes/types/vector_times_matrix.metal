// EXPECT: valid
// DISASM: OpVectorTimesMatrix %v2float
//
// A row vector times a matrix has as many components as the matrix has
// columns: a float3 times a float2x3 is a float2.
kernel void vector_times_matrix(device float2 *out [[buffer(0)]],
                                device const float2x3 *m [[buffer(1)]],
                                device const float3 *in [[buffer(2)]],
                                uint i [[thread_position_in_grid]])
{
    out[i] = in[i] * m[0];
}
