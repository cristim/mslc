// EXPECT: valid
// DISASM: OpCompositeConstruct %mat2v4float
kernel void matrix_from_columns(device float4 *out [[buffer(0)]],
                                device const float4 *columns [[buffer(1)]],
                                device const float2 *in [[buffer(2)]],
                                uint i [[thread_position_in_grid]])
{
    float2x4 m = float2x4(columns[0], columns[1]);
    out[i] = m * in[i];
}
