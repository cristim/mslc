// EXPECT: valid
// DISASM-MATCH: = OpAccessChain %_ptr_Function_v4float %[0-9]+ %int_1
// DISASM-MATCH: = OpCompositeExtract %float %[0-9]+ 2
//
// m[c][r]: the column is a vector, and its element is taken out of it (#53).
kernel void matrix_column_element_index(device float4 *out [[buffer(0)]],
                                        device const float4x4 *m [[buffer(1)]],
                                        device const float4 *in [[buffer(2)]],
                                        uint i [[thread_position_in_grid]])
{
    float4x4 local = m[0]; out[i] = float4(local[1][2]);
}
