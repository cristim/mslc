// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v4float %float_2 %float_2 %float_2 %float_2
// DISASM-MATCH: = OpFMul %v4float %[0-9]+ %[0-9]+
//
// float4 v; v *= 2.0f multiplies every lane by the scalar.
kernel void compound_vector_by_scalar_broadcasts(device float4 *out [[buffer(0)]],
                                                 uint i [[thread_position_in_grid]])
{
    float4 x = out[i];
    x *= 2.0f;
    out[i] = x;
}
