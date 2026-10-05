// EXPECT: error ".x" swizzles a value that is not a vector
//
// Apple reports "member reference base type 'float' is not a structure".
kernel void swizzle_of_a_scalar_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(a[i].x.x);
}
