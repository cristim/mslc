// EXPECT: valid
// DISASM: = OpCompositeConstruct %v3float
//
// `(float3)x` of a scalar broadcasts it, as float3(x) does.
kernel void cast_scalar_to_vector_broadcasts(device float3* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = (float3)in[0];
}
