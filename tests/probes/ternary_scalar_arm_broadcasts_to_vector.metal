// EXPECT: valid
// DISASM: = OpCompositeConstruct %v3float
// DISASM: = OpSelect %v3float
//
// A scalar arm beside a vector arm is converted to the vector's component type
// and broadcast; the result is the vector type.
kernel void ternary_scalar_arm_broadcasts_to_vector(device float3* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0], in[1], in[2]);
    float s = in[3];
    out[i] = in[4] > 0.0f ? v : s;
}
