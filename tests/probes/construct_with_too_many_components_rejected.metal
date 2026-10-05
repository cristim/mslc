// EXPECT: error float4 built from 10 components needs 4
//
// Apple reports "no matching constructor" for ten components of a float4.
kernel void construct_with_too_many_components_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(a[i], b[i], 0.0, 1.0);
}
