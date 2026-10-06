// EXPECT: valid
// DISASM-MATCH: = OpVectorExtractDynamic %float
//
// A run-time index of any integer width, or a bool, selects a lane (#53).
kernel void vector_element_dynamic_read(device float *out [[buffer(0)]],
                                        device const float4 *in [[buffer(1)]],
                                        device const uint *ix [[buffer(2)]])
{
    float4 v = in[0];
    uchar a = uchar(ix[0]);
    long b = long(ix[1]);
    out[0] = v[a] + v[b] + v[ix[2] != 0];
}
