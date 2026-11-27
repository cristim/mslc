// EXPECT: error the two values of a conditional are vectors of different types
//
// Apple: "implicit conversions between vector types ('float3' and 'int3') are
// not permitted".
kernel void ternary_vector_arms_of_different_types_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0]);
    int3 w = int3(i);
    float3 r = in[1] > 0.0f ? v : w;
    out[i] = r.x;
}
