// EXPECT: valid
// DISASM-MATCH: = OpVectorTimesMatrix %v4float %[0-9]+ %[0-9]+
//
// float4 v; v *= m is v = v * m.
kernel void compound_vector_by_matrix(device float4 *out [[buffer(0)]], device const float4x4 *m [[buffer(1)]],
                                      uint i [[thread_position_in_grid]])
{
    float4 x = out[i];
    x *= m[0];
    out[i] = x;
}
