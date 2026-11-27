// EXPECT: error assigning a vector of 2 components to ".x", which names one
//
// A single-component swizzle takes a scalar.
kernel void swizzle_store_vector_to_one_lane_rejected(device float4 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.x = b[i];
    out[i] = v;
}
