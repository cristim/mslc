// EXPECT: valid
// DISASM-MATCH: = OpSelect %uint
//
// Apple takes a bool as the index of a buffer element and of a matrix column;
// it is 0 or 1.
kernel void index_bool_buffer_and_matrix(device float4 *out [[buffer(0)]],
                                         device const float4 *in [[buffer(1)]],
                                         device const float4x4 *mats [[buffer(2)]],
                                         device const uint *ix [[buffer(3)]])
{
    bool t = ix[0] != 0;
    float4x4 m = mats[0];
    out[t] = in[t] + m[t];
}
