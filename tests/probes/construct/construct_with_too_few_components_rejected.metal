// EXPECT: error float4 built from 3 components needs 4
//
// Apple reports the call as ambiguous.
kernel void construct_with_too_few_components_rejected(device float4 *out [[buffer(0)]], constant float *s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(s[i], s[i], s[i]);
}
