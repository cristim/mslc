// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4float %float_2 %float_2 %float_2 %float_2
// DISASM-MATCH: = OpFMul %v4float %[0-9]+ %[0-9]+
//
// A scalar on the left of a vector: 2.0f * v. This used to be rejected as "a
// scalar cannot be converted to a vector".
kernel void arith_scalar_times_vector_broadcasts(device float4 *out [[buffer(0)]],
                                                 device const float4 *in [[buffer(1)]],
                                                 uint i [[thread_position_in_grid]])
{
    out[i] = 2.0f * in[i];
}
