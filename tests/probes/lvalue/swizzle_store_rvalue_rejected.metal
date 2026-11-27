// EXPECT: error assigning to ".xy" of a value that is not a local, a struct member or a buffer element
//
// Apple: "expression is not assignable".
kernel void swizzle_store_rvalue_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float2 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    (a[i] + a[i]).xy = b[i];
    out[i] = a[i];
}
