// EXPECT: error assigning to ".xy" takes a vector of 2 components of the same type as the target
//
// Apple: "assigning to 'float2' from incompatible type 'int2'".
kernel void swizzle_store_other_component_type_rejected(device float4 *out [[buffer(0)]], constant int2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xy = b[i];
    out[i] = v;
}
