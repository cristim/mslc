// EXPECT: error mslc converts no matrix to or from another type
// Apple rejects this implicit conversion too. The pin is that mslc reports it
// rather than handing a matrix to a convert opcode, none of which takes one.
kernel void matrix_conversion_rejected(device float4 *out [[buffer(0)]],
                                       device const float4x4 *m [[buffer(1)]],
                                       device const float4 *in [[buffer(2)]],
                                       uint i [[thread_position_in_grid]])
{
    half4x4 narrow = m[0]; out[i] = in[i];
}
