// EXPECT: valid
// DISASM: ArrayStride 32
// DISASM: ArrayStride 24
//
// An array of matrices steps by the column size times the column count: a
// float2x3 is two 16-byte columns, 32 bytes, and a float3x2 three 8-byte
// columns, 24. Multiplying by the row count instead gives 48 and 16, which a
// square matrix cannot tell apart.
kernel void matrix_buffer_stride_counts_columns(device float3 *out [[buffer(0)]],
                                                device const float2x3 *tall [[buffer(1)]],
                                                device const float3x2 *wide [[buffer(2)]],
                                                device const float2 *in [[buffer(3)]],
                                                device float2 *out2 [[buffer(4)]],
                                                device const float3 *in3 [[buffer(5)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = tall[i] * in[i];
    out2[i] = wide[i] * in3[i];
}
