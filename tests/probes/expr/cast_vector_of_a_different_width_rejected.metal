// EXPECT: error a vector of 3 components cannot be converted to one of 4
//
// Apple: "C-style cast from vector 'float3' to vector 'float4' of different size".
kernel void cast_vector_of_a_different_width_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float3 v = float3(in[0]);
    float4 r = (float4)v;
    out[i] = r.x;
}
