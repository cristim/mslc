// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v3float %float_1_5 %float_1_5 %float_1_5[^ _0-9a-zA-Z]
//
// Apple accepts static_cast<float3>(1.5f) and splats; so does the functional
// cast, and so does this.
kernel void static_cast_scalar_splats_to_vector(device float3* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<float3>(1.5f);
}
