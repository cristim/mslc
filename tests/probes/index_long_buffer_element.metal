// EXPECT: valid
// DISASM-MATCH: = OpUConvert %uint
//
// A 64-bit index is narrowed to the 32 bits an access chain index is.
kernel void index_long_buffer_element(device float *out [[buffer(0)]],
                                      device const float *in [[buffer(1)]],
                                      device const uint *ix [[buffer(2)]])
{
    long j = long(ix[0]);
    out[0] = in[j];
}
