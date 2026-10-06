// EXPECT: error the two values of a conditional are vectors of different widths
//
// Apple: "implicit conversions between vector types ('float2' and 'float3') are
// not permitted".
kernel void ternary_vector_arms_of_different_widths_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0]);
    float2 w = float2(in[1]);
    float3 r = in[2] > 0.0f ? v : w;
    out[i] = r.x;
}
